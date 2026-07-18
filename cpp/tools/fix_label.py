p = r'D:\latest-code\cpp\qml\SettingsPage.qml'
c = open(p, 'r', encoding='utf-8').read()
old = 'property string _label: items.length > 2 ? (_sel===0 ? items[0].t : (_sel===1 ? items[1].t : items[2].t)) : ""'
new = 'property string _label: _sel===0 ? items[0].t : (_sel===1 ? items[1].t : items[2].t)'
if old not in c:
    print("old not found, searching...")
    import re
    for m in re.finditer(r'property string _label:.*', c):
        print(repr(m.group()))
else:
    c = c.replace(old, new)
    open(p, 'w', encoding='utf-8', newline='\n').write(c)
    print('fixed')
