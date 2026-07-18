#!/usr/bin/env python3
p = 'D:/latest-code/cpp/qml/SettingsPage.qml'
with open(p, 'r', encoding='utf-8') as f:
    c = f.read()

# Replace dlThreads reference in ThreadVal text
c = c.replace(
    'text: Math.round(dlThreads)',
    'text: backend ? backend.maxDownloadThreads : 64'
)

# Replace onMoved handler for thread slider
c = c.replace(
    'onMoved: (nv) => {\n                        dlThreads = nv\n                        if (backend) backend.maxDownloadThreads = nv\n                    }',
    'onMoved: (nv) => { if (backend) backend.maxDownloadThreads = nv }'
)

# Replace dlSpeed in speedVal text
c = c.replace(
    'text: dlSpeed < 0 ? qsTr("无限制") : dlSpeed.toFixed(1) + " MB/s"',
    'text: (backend && backend.downloadSpeedLimitMB >= 0) ? backend.downloadSpeedLimitMB.toFixed(1) + " MB/s" : qsTr("无限制")'
)

# Replace onMoved handler for speed slider
c = c.replace(
    'onMoved: (nv) => {\n                        dlSpeed = _toSpeed(nv)\n                        if (backend) backend.downloadSpeedLimitMB = dlSpeed',
    'onMoved: (nv) => { if (backend) backend.downloadSpeedLimitMB = _toSpeed(nv)'
)

with open(p, 'w', encoding='utf-8', newline='\n') as f:
    f.write(c)
print('done')
