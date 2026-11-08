import json,pathlib,re,sys
path=pathlib.Path(sys.argv[1])
meta=json.loads(path.read_text())
header=(pathlib.Path(__file__).resolve().parent/'../Source/Patch.h').read_text()
body=re.search(r'enum Param\s*:\s*int\s*\{(.*?)\};',header,re.S)[1]
values={}
value=-1
for item in body.split(','):
    item=item.strip()
    if not item: continue
    if '=' in item:
        key,expression=item.split('=',1)
        value=eval(expression.strip(),{'__builtins__':{}},values)
    else:
        key=item
        value+=1
    values[key.strip()]=value
meta['P']=values
path.write_text(json.dumps(meta,separators=(',',':')))
