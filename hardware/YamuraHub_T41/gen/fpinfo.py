import pcbnew, sys
L=r'C:/Program Files/KiCad/10.0/share/kicad/footprints/'
def show(lib,name):
    fp=pcbnew.FootprintLoad(L+lib,name); print(name)
    print('  pads',[(p.GetNumber(),round(p.GetPosition().x/1e6,2),round(p.GetPosition().y/1e6,2),round(p.GetSize().x/1e6,2),round(p.GetDrillSize().x/1e6,2)) for p in fp.Pads()])
    xs=[];ys=[]
    for g in fp.GraphicalItems():
        if g.GetLayer()==pcbnew.F_CrtYd:
            b=g.GetBoundingBox(); xs+=[b.GetLeft()/1e6,b.GetRight()/1e6]; ys+=[b.GetTop()/1e6,b.GetBottom()/1e6]
    print('  crtyd x',round(min(xs),2),round(max(xs),2),'y',round(min(ys),2),round(max(ys),2))
for a in sys.argv[1:]:
    lib,name=a.split(':'); show(lib+'.pretty',name)
