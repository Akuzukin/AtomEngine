"""Deterministic primitive scene and synthesized WAV assets. No network dependencies."""
from pathlib import Path
import xml.etree.ElementTree as E
import math, wave, struct, random
root=Path(__file__).resolve().parents[1]
scene=E.Element('Scene',version='1',name='碎星哨站 / survivor-v1')
env=E.SubElement(scene,'Environment',sky='',skyMode='0',skyIntensity='.25',sunIntensity='1',diffuse='',specular='',brdf='',intensity='.6',exposure='1',enabled='true',fxaa='true',autoExposure='false')
for name,c in [('Zenith',(.015,.025,.07)),('Horizon',(.04,.10,.18)),('Ground',(.015,.02,.035))]: E.SubElement(env,name,x=str(c[0]),y=str(c[1]),z=str(c[2]))
def obj(name,pos=(0,0,0),scale=(1,1,1),rotation=(0,0,0,1)):
    o=E.SubElement(scene,'Object',id=name,name=name,active='true');t=E.SubElement(o,'Transform')
    for tag,v,keys in [('Position',pos,'xyz'),('Rotation',rotation,'xyzw'),('Scale',scale,'xyz')]:E.SubElement(t,tag,**dict(zip(keys,map(str,v))))
    return o
def script(o,name,**fields):
    s=E.SubElement(o,'Script',assembly='Scripts/AtomScripts.dll',type='StarOutpost.'+name,enabled='true')
    for key,value in fields.items():E.SubElement(s,'Field',name=key,value=str(value).lower() if isinstance(value,bool) else str(value))
def mesh(name,pos=(0,-100,0),scale=(1,1,1),color=(.1,.8,1),primitive=2,visible=False,glow=.25):
    o=obj(name,pos,scale);E.SubElement(o,'Mesh',primitive=str(primitive),asset='',visible=str(visible).lower())
    m=E.SubElement(o,'Material',metallic='.2',roughness='.7',normalScale='1',ao='1',useLight='1',shadingModel='0')
    E.SubElement(m,'BaseColor',x=str(color[0]),y=str(color[1]),z=str(color[2]),alpha='1');E.SubElement(m,'Emissive',x=str(color[0]*glow),y=str(color[1]*glow),z=str(color[2]*glow));return o
def ui(name,x,y,w,h,text='',font=24,color=(.75,.9,1),bg=(.025,.055,.10,.96),layer=12,button=None,bar=False):
    o=obj(name);u=E.SubElement(o,'UI',x=str(x),y=str(y),width=str(w),height=str(h),text=text,texture='',fontSize=str(font),responsive='true',progressBar=str(bar).lower(),progress='1',interactable=str(button is not None).lower(),visible='true',layer=str(layer),anchorMinX='.5',anchorMinY='.5',anchorMaxX='.5',anchorMaxY='.5',pivotX='0',pivotY='0',padding='16')
    if name in ['HUD','Health','XP','Energy']:
        u.set('anchorMinX','0');u.set('anchorMinY','0');u.set('anchorMaxX','0');u.set('anchorMaxY','0');u.set('x',str(x+960));u.set('y',str(y+540))
    if name=='Footer':
        u.set('anchorMinX','0');u.set('anchorMaxX','0');u.set('anchorMinY','1');u.set('anchorMaxY','1');u.set('x',str(x+960));u.set('y',str(y-540))
    if name=='Mute':
        u.set('anchorMinX','1');u.set('anchorMaxX','1');u.set('anchorMinY','0');u.set('anchorMaxY','0');u.set('x',str(x-960));u.set('y',str(y+540))
    E.SubElement(u,'Background',x=str(bg[0]),y=str(bg[1]),z=str(bg[2]),alpha=str(bg[3]));E.SubElement(u,'Color',x=str(color[0]),y=str(color[1]),z=str(color[2]),alpha='1')
    if button:
        s=E.SubElement(o,'Script',assembly='Scripts/AtomScripts.dll',type='StarOutpost.OutpostButton',enabled='true');E.SubElement(s,'Field',name='Action',value=button)
    return o
o=obj('Controller');E.SubElement(o,'Script',assembly='Scripts/AtomScripts.dll',type='StarOutpost.OutpostRuntime',enabled='true')
for name in ['OutpostUI','EnemySpawner','ProjectileSpawner','DropSpawner','AudioController']:script(obj(name),name)
script(obj('EffectSpawner'),'EffectSpawner',**dict(zip(['HitEffect','UpgradeEffect','DeathEffect','HealEffect'],['particle:BurstEmitter'+str(i) for i in range(4)])))
for i in range(4):
    emitter=obj('BurstEmitter'+str(i));E.SubElement(emitter,'ParticleComponent',preset='Asset/Particles/Burst'+str(i)+'.json',playOnAwake='false')
    import json
    particle_dir=root/'Asset/Particles';particle_dir.mkdir(parents=True,exist_ok=True)
    color=[(1,1,1),(.2,.7,1),(1,.4,.1),(.1,1,.3)][i]
    (particle_dir/('Burst'+str(i)+'.json')).write_text(json.dumps(dict(version=2,capacity=128,burst=32,rate=160,duration=.45,lifetime=.45,sizeStart=.09,sizeEnd=0,speed=2,shape=2,radius=.2,blend=1,billboard=True,loop=False,texture='Asset/Textures/Particle.png',colorStart=[*color,1],colorEnd=[*color,0],gravity=[0,0,0])),encoding='utf-8')
for i in range(7):script(obj('Weapon'+str(i)),'WeaponController',WeaponIndex=i)
angle=math.atan2(17,12);o=obj('Camera',(0,17,-12),rotation=(math.sin(angle/2),0,0,math.cos(angle/2)));E.SubElement(o,'Camera',fieldOfView='55',nearClip='.1',farClip='200',primary='true',enabled='true')
script(o,'CameraController')
o=obj('Sun',rotation=(.38,-.2,0,.903));l=E.SubElement(o,'DirectionalLight',intensity='2',enabled='true',shadows='true',autoFitShadow='true');E.SubElement(l,'Color',x='.65',y='.8',z='1')
mesh('Arena',(0,-.25,0),(24,.4,16),(.035,.065,.1),visible=True,glow=.02)
for i in range(-12,13):mesh('GridX'+str(i),(i,-.025,0),(.015,.015,16),(.06,.19,.25),visible=True)
for i in range(-8,9):mesh('GridZ'+str(i),(0,-.025,i),(24,.015,.015),(.06,.19,.25),visible=True)
for i,(x,z,sx,sz) in enumerate([(0,-8,24,.15),(0,8,24,.15),(-12,0,.15,16),(12,0,.15,16)]):mesh('Boundary'+str(i),(x,.06,z),(sx,.12,sz),(.08,.65,.7),visible=True,glow=1)
rng=random.Random(19)
for i in range(48):
    a=rng.random()*math.tau;radius=16+rng.random()*12;mesh('Asteroid'+str(i),(math.cos(a)*radius,-1+rng.random()*4,math.sin(a)*radius),(1+rng.random()*2,1+rng.random()*2,1+rng.random()*2),(.07,.10,.17),visible=True,glow=.02)
script(mesh('Player',color=(.06,.85,1),visible=False),'PlayerController')
script(mesh('Supply',color=(.1,1,.6),visible=False),'SupplyController')
script(mesh('BlackHole',color=(.2,.04,.5),visible=False),'BlackHoleController')
# Templates are resources, not scene entities. Every live instance owns its script.
prefabdir=root/'Asset/Prefabs';prefabdir.mkdir(parents=True,exist_ok=True)
for name,controller,color in [('Enemy','EnemyController',(.9,.18,.3)),('Projectile','ProjectileController',(.2,.9,1)),('Drop','DropController',(.1,1,.7)),('Effect','EffectController',(1,.7,.25)),('OrbitBlade','OrbitBladeController',(.55,.4,1)),('Telegraph','TelegraphController',(1,.06,.08)),('Corpse','CorpseController',(.5,.18,.08))]:
    template=mesh(name,color=color,glow=1);script(template,controller);scene.remove(template)
    prefab=E.Element('Prefab',version='1',root=name);prefab.append(template);E.indent(prefab);E.ElementTree(prefab).write(prefabdir/(name+'.prefab'),encoding='utf-8',xml_declaration=True)
ui('HUD',-900,-500,1350,80,font=24,bg=(.015,.03,.06,.88),layer=3)
ui('Health',-900,-400,310,14,color=(.2,1,.6),bar=True,layer=3)
ui('XP',-570,-400,310,14,color=(.3,.6,1),bar=True,layer=3)
ui('Energy',-240,-400,210,14,color=(.7,.4,1),bar=True,layer=3)
ui('Boss',-350,-365,700,12,color=(1,.15,.25),bar=True,layer=4)
ui('BossText',-300,-345,600,38,font=22,layer=4)
ui('Notice',-650,-290,1300,55,font=24,color=(1,.8,.35),layer=30)
ui('Footer',-900,460,1750,50,font=22,layer=3)
ui('Panel',-790,-320,1580,750,layer=10,bg=(.014,.025,.055,.97))
ui('Title',-720,-290,1440,110,font=38,color=(.4,.9,1),bg=(0,0,0,0))
ui('Body',-720,-165,1440,400,font=25,bg=(0,0,0,0))
ui('Primary',-720,290,610,68,font=26,bg=(.04,.25,.32,1),button='primary')
ui('Secondary',20,290,650,68,font=25,bg=(.15,.10,.25,1),button='secondary')
ui('Back',-120,365,240,50,font=22,button='back')
for i in range(3):ui('Choice'+str(i),-710+i*480,-55,440,300,font=24,bg=(.035,.10,.18,1),button='choice'+str(i))
for i in range(15):ui('Shop'+str(i),-720+(i%5)*290,-65+(i//5)*110,260,90,font=22,button='shop'+str(i))
ui('Mute',650,-500,260,50,font=21,layer=40,button='mute')
template=ui('DamageNumber',0,0,90,36,font=21,color=(1,.9,.5),bg=(0,0,0,0),layer=5);script(template,'DamageNumberController');scene.remove(template)
prefab=E.Element('Prefab',version='1',root='DamageNumber');prefab.append(template);E.ElementTree(prefab).write(prefabdir/'DamageNumber.prefab',encoding='utf-8',xml_declaration=True)
(root/'Asset/Scenes').mkdir(parents=True,exist_ok=True);E.indent(scene);E.ElementTree(scene).write(root/'Asset/Scenes/Outpost.scene',encoding='utf-8',xml_declaration=True)
# Diagnostic scene is separate; production entry never enables auto play or invulnerability.
script(obj('Diagnostics'),'GameDiagnostics',Stress=True);E.ElementTree(scene).write(root/'Asset/Scenes/RuntimeCheck.scene',encoding='utf-8',xml_declaration=True)
# A second diagnostic follows the actual low-density opening rather than a capacity stress setup.
scene.find("Object[@id='Diagnostics']/Script/Field[@name='Stress']").set('value','false');E.ElementTree(scene).write(root/'Asset/Scenes/GameplayCheck.scene',encoding='utf-8',xml_declaration=True)
sounddir=root/'Asset/Sounds';sounddir.mkdir(parents=True,exist_ok=True)
for name,duration,freq in [('shot',.07,760),('hurt',.2,95),('upgrade',.6,440),('warning',.7,180),('charge',.35,260),('storm',.4,80)]:
    samples=[]
    for i in range(int(22050*duration)):
        t=i/22050;envelope=(1-t/duration)**2;f=freq*(1+(t/duration)*(.8 if name=='upgrade' else -.4));value=math.sin(math.tau*f*t)*envelope*.35
        if name in ('hurt','storm'):value+=(rng.random()*2-1)*envelope*.12
        samples.append(struct.pack('<h',int(max(-1,min(1,value))*32767)))
    with wave.open(str(sounddir/(name+'.wav')),'wb') as wav:wav.setnchannels(1);wav.setsampwidth(2);wav.setframerate(22050);wav.writeframes(b''.join(samples))
print('Generated scene, diagnostic scene and six synthesized sounds')
