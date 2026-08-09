// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#include "webview2_window.h"
#include "../utils/logger.h"

#include <windows.h>
#include <WebView2.h>

#include <QResizeEvent>
#include <QShowEvent>

namespace ShadowLauncher {

// ── Minimal COM callback wrappers (WRL Callback was removed from newer
//    Windows SDKs; these are drop-in replacements, zero extra dependencies) ──

namespace {

template <typename Interface>
class ComHandlerBase : public Interface {
public:
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        *ppv = nullptr;
        if (riid == __uuidof(IUnknown) || riid == __uuidof(Interface)) {
            *ppv = static_cast<Interface*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return ++m_refs; }
    STDMETHODIMP_(ULONG) Release() override {
        const ULONG r = --m_refs;
        if (r == 0) delete this;
        return r;
    }
protected:
    ULONG m_refs = 1;
};

template <typename Fn>
class EnvCompletedHandler final
    : public ComHandlerBase<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler> {
public:
    explicit EnvCompletedHandler(Fn fn) : m_fn(std::move(fn)) {}
    STDMETHODIMP Invoke(HRESULT result, ICoreWebView2Environment* createdEnvironment) override {
        return m_fn(result, createdEnvironment);
    }
private:
    Fn m_fn;
};

template <typename Fn>
class ControllerCompletedHandler final
    : public ComHandlerBase<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler> {
public:
    explicit ControllerCompletedHandler(Fn fn) : m_fn(std::move(fn)) {}
    STDMETHODIMP Invoke(HRESULT result, ICoreWebView2Controller* createdController) override {
        return m_fn(result, createdController);
    }
private:
    Fn m_fn;
};

template <typename Fn>
class NavigationStartingHandler final
    : public ComHandlerBase<ICoreWebView2NavigationStartingEventHandler> {
public:
    explicit NavigationStartingHandler(Fn fn) : m_fn(std::move(fn)) {}
    STDMETHODIMP Invoke(ICoreWebView2* sender, ICoreWebView2NavigationStartingEventArgs* args) override {
        Q_UNUSED(sender)
        return m_fn(args);
    }
private:
    Fn m_fn;
};

template <typename Fn>
class NavigationCompletedHandler final
    : public ComHandlerBase<ICoreWebView2NavigationCompletedEventHandler> {
public:
    explicit NavigationCompletedHandler(Fn fn) : m_fn(std::move(fn)) {}
    STDMETHODIMP Invoke(ICoreWebView2* sender, ICoreWebView2NavigationCompletedEventArgs* args) override {
        Q_UNUSED(sender)
        return m_fn(args);
    }
private:
    Fn m_fn;
};

} // namespace

// ── WebView2Window ──

struct WebView2Window::Impl {
    ICoreWebView2Controller* controller = nullptr;
    ICoreWebView2* webview = nullptr;
    EventRegistrationToken navStartingToken{};
    EventRegistrationToken navCompletedToken{};
    WebView2Window::NavigationInterceptor interceptor;
    QString browserFolder;
    QUrl pendingUrl;
    bool envInitFailed = false;
    bool envCreating = false;
};

WebView2Window::WebView2Window(QWidget* parent)
    : QWidget(parent), m_impl(std::make_unique<Impl>()) {
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(tr("Microsoft 登录"));
    resize(480, 650);
}

WebView2Window::~WebView2Window() {
    // Order matters: unsubscribe events, Close the controller, then release.
    // The WebView2 child HWND is destroyed by Close().
    if (m_impl && m_impl->controller) {
        if (m_impl->webview) {
            m_impl->webview->remove_NavigationStarting(m_impl->navStartingToken);
            m_impl->webview->remove_NavigationCompleted(m_impl->navCompletedToken);
        }
        m_impl->controller->Close();
        m_impl->controller->Release();
        m_impl->controller = nullptr;
        m_impl->webview = nullptr;
    }
}

void WebView2Window::loadUrl(const QUrl& url) {
    m_impl->pendingUrl = url;
    if (m_impl->webview) {
        navigate(url);
    }
    // Not ready yet — creation happens in showEvent, then pendingUrl is navigated.
}

void WebView2Window::navigate(const QUrl& url) {
    if (!m_impl->webview) return;
    const std::wstring wurl = url.toString().toStdWString();
    const HRESULT hr = m_impl->webview->Navigate(wurl.c_str());
    qCInfo(logApp) << QStringLiteral("[WebView2] Navigate hr=0x%1 url=%2")
                          .arg(static_cast<quint32>(hr), 0, 16)
                          .arg(url.toString().left(90));
}

void WebView2Window::setNavigationInterceptor(NavigationInterceptor interceptor) {
    m_impl->interceptor = std::move(interceptor);
}

void WebView2Window::setBrowserExecutableFolder(const QString& folder) {
    m_impl->browserFolder = folder;
}

bool WebView2Window::isReady() const {
    return m_impl->webview != nullptr;
}

QString WebView2Window::currentUrl() const {
    if (!m_impl->webview) return {};
    PWSTR uri = nullptr;
    if (FAILED(m_impl->webview->get_Source(&uri)) || !uri) return {};
    const QString url = QString::fromWCharArray(uri);
    CoTaskMemFree(uri);
    return url;
}

void WebView2Window::createWebView2() {
    m_impl->envCreating = true;
    // WebView2 requires an STA thread. QApplication initializes OLE on the main
    // thread, but be explicit (idempotent — S_FALSE if already initialized).
    const HRESULT cohr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (cohr == RPC_E_CHANGED_MODE) {
        qCWarning(logApp) << QStringLiteral("[WebView2] COM 已是其他线程模式 (MTA)，可能影响回调派发");
    }
    qCInfo(logApp) << QStringLiteral("[WebView2] createWebView2: 创建环境 (CoInit=0x%1)")
                          .arg(static_cast<quint32>(cohr), 0, 16);

    // Async: environment → controller → webview. All callbacks arrive on the
    // calling thread's message loop (main thread), so Qt signal emission here
    // is safe without queued connections.
    const std::wstring folderW = m_impl->browserFolder.toStdWString();
    const HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        m_impl->browserFolder.isEmpty() ? nullptr : folderW.c_str(),
        nullptr, nullptr,
        new EnvCompletedHandler([this](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
            qCInfo(logApp) << QStringLiteral("[WebView2] 环境回调 result=0x%1 env=%2")
                                  .arg(static_cast<quint32>(result), 0, 16)
                                  .arg(env ? QStringLiteral("OK") : QStringLiteral("null"));
            if (FAILED(result) || !env) {
                m_impl->envInitFailed = true;
                m_impl->envCreating = false;
                const QString msg = QStringLiteral("WebView2 运行时创建失败 (0x%1)")
                                        .arg(static_cast<quint32>(result), 0, 16);
                qCWarning(logAccount) << QStringLiteral("[WebView2] %1").arg(msg);
                emit environmentError(msg);
                return S_OK;
            }
            qCInfo(logApp) << QStringLiteral("[WebView2] 环境就绪，创建控制器 hwnd=%1")
                                  .arg(static_cast<qulonglong>(winId()));
            env->CreateCoreWebView2Controller(
                reinterpret_cast<HWND>(winId()),
                new ControllerCompletedHandler(
                    [this](HRESULT res2, ICoreWebView2Controller* controller) -> HRESULT {
                        m_impl->envCreating = false;
                        qCInfo(logApp) << QStringLiteral("[WebView2] 控制器回调 result=0x%1 controller=%2")
                                              .arg(static_cast<quint32>(res2), 0, 16)
                                              .arg(controller ? QStringLiteral("OK") : QStringLiteral("null"));
                        if (FAILED(res2) || !controller) {
                            m_impl->envInitFailed = true;
                            const QString msg = QStringLiteral("WebView2 控制器创建失败 (0x%1)")
                                                    .arg(static_cast<quint32>(res2), 0, 16);
                            qCWarning(logAccount) << QStringLiteral("[WebView2] %1").arg(msg);
                            emit environmentError(msg);
                            return S_OK;
                        }
                        m_impl->controller = controller;
                        if (FAILED(controller->get_CoreWebView2(&m_impl->webview))) {
                            m_impl->envInitFailed = true;
                            const QString msg = QStringLiteral("WebView2 获取核心接口失败");
                            qCWarning(logAccount) << QStringLiteral("[WebView2] %1").arg(msg);
                            emit environmentError(msg);
                            return S_OK;
                        }
                        m_impl->webview->add_NavigationStarting(
                            new NavigationStartingHandler(
                                [this](ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT {
                                    PWSTR uri = nullptr;
                                    if (SUCCEEDED(args->get_Uri(&uri)) && uri) {
                                        const QString urlStr = QString::fromWCharArray(uri);
                                        CoTaskMemFree(uri);
                                        qCInfo(logApp) << QStringLiteral("[WebView2] 导航: %1").arg(urlStr.left(100));
                                        const bool blocked = m_impl->interceptor
                                            && m_impl->interceptor(urlStr);
                                        if (blocked) {
                                            args->put_Cancel(TRUE);
                                            emit navigationBlocked(urlStr);
                                        }
                                    }
                                    return S_OK;
                                }),
                            &m_impl->navStartingToken);
                        m_impl->webview->add_NavigationCompleted(
                            new NavigationCompletedHandler(
                                [this](ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT {
                                    BOOL ok = FALSE;
                                    args->get_IsSuccess(&ok);
                                    qCInfo(logApp) << QStringLiteral("[WebView2] 导航完成 ok=%1").arg(ok == TRUE ? 1 : 0);
                                    emit loadFinished(ok == TRUE);
                                    return S_OK;
                                }),
                            &m_impl->navCompletedToken);
                        updateBounds();
                        emit initialized();
                        if (!m_impl->pendingUrl.isEmpty()) {
                            navigate(m_impl->pendingUrl);
                        }
                        return S_OK;
                    }));
            return S_OK;
        }));
    if (FAILED(hr)) {
        m_impl->envCreating = false;
        m_impl->envInitFailed = true;
        const QString msg = QStringLiteral("WebView2 环境启动失败 (0x%1)")
                                .arg(static_cast<quint32>(hr), 0, 16);
        qCWarning(logAccount) << QStringLiteral("[WebView2] %1").arg(msg);
        emit environmentError(msg);
    }
}

void WebView2Window::updateBounds() {
    if (!m_impl->controller) return;
    const RECT bounds{0, 0, width(), height()};
    m_impl->controller->put_Bounds(bounds);
}

void WebView2Window::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    updateBounds();
}

void WebView2Window::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (m_impl->controller) {
        updateBounds();
    } else if (!m_impl->envInitFailed && !m_impl->envCreating) {
        // Create after the window is actually visible — matches the official
        // WebView2 sample flow and guarantees a valid HWND + real bounds.
        createWebView2();
    }
}

} // namespace ShadowLauncher
