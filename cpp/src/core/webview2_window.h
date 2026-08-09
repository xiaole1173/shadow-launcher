// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#pragma once

// ── Embedded browser via Edge WebView2 (system Chromium runtime) ──
// Drop-in replacement for QWebEngineView-based embedded login (Mode B).
// Adds ~200KB to the exe (static WebView2Loader), zero extra DLLs shipped —
// uses the WebView2 Runtime that ships with Windows 10/11 / Edge.

#include <QWidget>
#include <QUrl>
#include <functional>
#include <memory>

namespace ShadowLauncher {

class WebView2Window : public QWidget {
    Q_OBJECT
public:
    explicit WebView2Window(QWidget* parent = nullptr);
    ~WebView2Window() override;

    void loadUrl(const QUrl& url);
    bool isReady() const;
    QString currentUrl() const;

    // Returns true to cancel (block) the navigation. Used to capture the OAuth
    // redirect before the page actually navigates away from the callback URL.
    using NavigationInterceptor = std::function<bool(const QString& url)>;
    void setNavigationInterceptor(NavigationInterceptor interceptor);

    // Pin the WebView2 Runtime browser executable folder (diagnostic; default
    // = system-selected Evergreen runtime).
    void setBrowserExecutableFolder(const QString& folder);

signals:
    void initialized();
    void loadFinished(bool ok);
    void navigationBlocked(const QString& url);
    void environmentError(const QString& message);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    void createWebView2();
    void updateBounds();
    void navigate(const QUrl& url);

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace ShadowLauncher
