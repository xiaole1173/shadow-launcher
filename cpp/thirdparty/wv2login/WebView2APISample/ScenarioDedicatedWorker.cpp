// Copyright (C) Microsoft Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stdafx.h"

#include "CheckFailure.h"
#include "ScenarioDedicatedWorker.h"

using namespace Microsoft::WRL;

ScenarioDedicatedWorker::ScenarioDedicatedWorker(AppWindow* appWindow) : m_appWindow(appWindow)
{
    //! [DedicatedWorkerCreated]
    m_appWindow->GetWebView()->QueryInterface(IID_PPV_ARGS(&m_webView2_29));
    CHECK_FEATURE_RETURN_EMPTY(m_webView2_29);

    CHECK_FAILURE(m_webView2_29->add_DedicatedWorkerCreated(
        Callback<ICoreWebView2DedicatedWorkerCreatedEventHandler>(
            [this](ICoreWebView2* sender, ICoreWebView2DedicatedWorkerCreatedEventArgs* args)
            {
                wil::com_ptr<ICoreWebView2DedicatedWorker> dedicatedWorker;
                CHECK_FAILURE(args->get_Worker(&dedicatedWorker));

                wil::unique_cotaskmem_string scriptUri;
                CHECK_FAILURE(dedicatedWorker->get_ScriptUri(&scriptUri));

                std::wstring scriptUriStr(scriptUri.get());
                m_appWindow->AsyncMessageBox(scriptUriStr, L"Dedicated worker is created");

                // Subscribe to worker destroying event
                dedicatedWorker->add_Destroying(
                    Callback<ICoreWebView2DedicatedWorkerDestroyingEventHandler>(
                        [this, scriptUriStr](
                            ICoreWebView2DedicatedWorker* sender, IUnknown* args) -> HRESULT
                        {
                            /*Cleanup on worker destruction*/
                            m_appWindow->AsyncMessageBox(
                                scriptUriStr, L"Dedicated worker is destroyed");
                            return S_OK;
                        })
                        .Get(),
                    nullptr);

                return S_OK;
            })
            .Get(),
        &m_dedicatedWorkerCreatedToken));
    //! [DedicatedWorkerCreated]

    wil::com_ptr<ICoreWebView2_4> m_webView2_4;
    m_appWindow->GetWebView()->QueryInterface(IID_PPV_ARGS(&m_webView2_4));
    CHECK_FEATURE_RETURN_EMPTY(m_webView2_4);

    CHECK_FAILURE(m_webView2_4->add_FrameCreated(
        Callback<ICoreWebView2FrameCreatedEventHandler>(
            [this](ICoreWebView2* sender, ICoreWebView2FrameCreatedEventArgs* args) -> HRESULT
            {
                wil::com_ptr<ICoreWebView2Frame> webviewFrame;
                CHECK_FAILURE(args->get_Frame(&webviewFrame));

                wil::com_ptr<ICoreWebView2Frame8> m_frame8 =
                    webviewFrame.try_query<ICoreWebView2Frame8>();

                m_frame8->add_DedicatedWorkerCreated(
                    Callback<ICoreWebView2FrameDedicatedWorkerCreatedEventHandler>(
                        [this](
                            ICoreWebView2Frame* sender,
                            ICoreWebView2DedicatedWorkerCreatedEventArgs* args) -> HRESULT
                        {
                            // frameInfo that created the worker.
                            wil::com_ptr<ICoreWebView2FrameInfo> frameInfo;
                            CHECK_FAILURE(args->get_OriginalSourceFrameInfo(&frameInfo));

                            wil::com_ptr<ICoreWebView2FrameInfo2> frameInfo2;
                            CHECK_FAILURE(frameInfo->QueryInterface(IID_PPV_ARGS(&frameInfo2)));

                            wil::unique_cotaskmem_string frameSource;
                            CHECK_FAILURE(frameInfo->get_Source(&frameSource));

                            UINT32 source_frameId;
                            CHECK_FAILURE(frameInfo2->get_FrameId(&source_frameId));

                            wil::com_ptr<ICoreWebView2DedicatedWorker> dedicatedWorker;
                            CHECK_FAILURE(args->get_Worker(&dedicatedWorker));

                            wil::unique_cotaskmem_string scriptUri;
                            CHECK_FAILURE(dedicatedWorker->get_ScriptUri(&scriptUri));

                            std::wstring scriptUriStr(scriptUri.get());
                            m_appWindow->AsyncMessageBox(
                                scriptUriStr, L"Dedicated worker is created");

                            // Subscribe to worker destroying event
                            dedicatedWorker->add_Destroying(
                                Callback<ICoreWebView2DedicatedWorkerDestroyingEventHandler>(
                                    [this, scriptUriStr](
                                        ICoreWebView2DedicatedWorker* sender,
                                        IUnknown* args) -> HRESULT
                                    {
                                        /*Cleanup on worker destruction*/
                                        m_appWindow->AsyncMessageBox(
                                            scriptUriStr, L"Dedicated worker is destroyed");
                                        return S_OK;
                                    })
                                    .Get(),
                                nullptr);

                            return S_OK;
                        })
                        .Get(),
                    nullptr);

                return S_OK;
            })
            .Get(),
        nullptr));
}

ScenarioDedicatedWorker::~ScenarioDedicatedWorker()
{
    m_webView2_29->remove_DedicatedWorkerCreated(m_dedicatedWorkerCreatedToken);
}
