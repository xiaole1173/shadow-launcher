// Copyright (C) Microsoft Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stdafx.h"

#include "AppWindow.h"
#include "ComponentBase.h"

class ScenarioDedicatedWorkerPostMessage : public ComponentBase
{
public:
    ScenarioDedicatedWorkerPostMessage(AppWindow* appWindow);
    ~ScenarioDedicatedWorkerPostMessage() override;

private:
    void SetupEventsOnDedicatedWorker(
        wil::com_ptr<ICoreWebView2DedicatedWorker> dedicatedWorker);
    void ComputeWithDedicatedWorker(wil::com_ptr<ICoreWebView2DedicatedWorker> dedicatedWorker);

    AppWindow* m_appWindow;
    wil::com_ptr<ICoreWebView2_29> m_webView2_29;
    std::wstring m_sampleUri;
    EventRegistrationToken m_contentLoadingToken = {};
    EventRegistrationToken m_dedicatedWorkerCreatedToken = {};
};
