# Designs + verifies the polyphase IIR half-band filters (oversampling) and the Hilbert allpass pair.
import numpy as np, math
def halfband(att_db, transition):
    k = math.tan((1 - transition*2) * math.pi/4); k *= k
    kk = (1 - k*k) ** 0.25
    e = 0.5*(1-kk)/(1+kk); e4 = e**4
    q = e*(1 + e4*(2 + e4*(15 + 150*e4)))
    a2 = 10**(-att_db/10); a = a2/(1-a2)
    order = math.ceil(math.log(a*a/16)/math.log(q))
    if order % 2 == 0: order += 1
    if order == 1: order = 3
    n = (order-1)//2; coefs=[]
    for idx in range(n):
        c = idx+1
        num=0; i=0; j=1
        while True:
            t = q**(i*(i+1))*math.sin((i*2+1)*c*math.pi/order)*j; num += t; j=-j; i+=1
            if abs(t) < 1e-100: break
        num *= q**0.25
        den=0; i=1; j=-1
        while True:
            t = q**(i*i)*math.cos(i*2*c*math.pi/order)*j; den += t; j=-j; i+=1
            if abs(t) < 1e-100: break
        den += 0.5
        ww = (num/den)**2
        x = math.sqrt((1-ww*k)*(1-ww/k))/(1+ww)
        coefs.append((1-x)/(1+x))
    return coefs
class AP:
    def __init__(s,a): s.a=a; s.x1=0; s.y1=0
    def tick(s,x):
        y = s.a*(x - s.y1) + s.x1; s.x1=x; s.y1=y; return y
def up(coefs, x):
    A=[AP(c) for c in coefs[0::2]]; B=[AP(c) for c in coefs[1::2]]; out=[]
    for v in x:
        a=v
        for f in A: a=f.tick(a)
        b=v
        for f in B: b=f.tick(b)
        out += [a,b]
    return np.array(out)
def down(coefs, y):
    A=[AP(c) for c in coefs[0::2]]; B=[AP(c) for c in coefs[1::2]]; out=[]
    for i in range(0,len(y),2):
        a=y[i+1]
        for f in A: a=f.tick(a)
        b=y[i]
        for f in B: b=f.tick(b)
        out.append(0.5*(a+b))
    return np.array(out)
if __name__ == "__main__":
    for name,att,tr in [("kHb1",80,0.06),("kHb2",70,0.20)]:
        c = halfband(att,tr)
        x=np.zeros(4096); x[0]=1
        H=np.abs(np.fft.rfft(up(c,x)))/2; f=np.linspace(0,1,len(H))   # 1 = fs_high/2
        pb = 20*np.log10(H[f < 0.5-tr*1.0].min()+1e-12); sb = 20*np.log10(H[f > 0.5+tr*1.0].max()+1e-12)
        # down: feed white-ish impulse at high rate
        y=np.zeros(8192); y[0]=1; D=np.abs(np.fft.rfft(np.repeat(down(c,y),2)))  # crude
        yh=np.zeros(8192); n=np.arange(8192); tone=np.sin(2*np.pi*0.40*n)  # 0.40 of high fs -> must be removed by downsampler
        d=down(c,tone); rej=20*np.log10(np.sqrt(np.mean(d[2000:]**2))/0.707+1e-12)
        tone2=np.sin(2*np.pi*0.05*n); d2=down(c,tone2); keep=20*np.log10(np.sqrt(np.mean(d2[2000:]**2))/0.707)
        print(f"static const float {name}[{len(c)}] = {{ " + ", ".join(f"{v:.10f}f" for v in c) + f" }};  // up: passband min {pb:.2f} dB, stopband max {sb:.1f} dB; down: reject {rej:.1f} dB, keep {keep:.2f} dB")
    # Hilbert pair (Olli Niemitalo): check 90 degree difference
    a1=[0.6923878, 0.9360654322959, 0.9882295226860, 0.9987488452737]; a2=[0.4021921162426, 0.8561710882420, 0.9722909545651, 0.9952884791278]
    def path(coefs, x, delay):
        y=x.copy()
        for a in coefs:
            o=np.zeros_like(y); a2_=a*a
            for n in range(len(y)):
                o[n] = a2_*(y[n] + (o[n-2] if n>=2 else 0)) - (y[n-2] if n>=2 else 0)
            y=o
        if delay: y=np.concatenate([[0],y[:-1]])
        return y
    x=np.zeros(16384); x[0]=1
    P1=np.fft.rfft(path(a1,x,True)); P2=np.fft.rfft(path(a2,x,False)); fr=np.fft.rfftfreq(16384,1/48000)
    d=np.degrees(np.angle(P1/P2)); m=(fr>40)&(fr<20000)
    print("Hilbert phase diff 40Hz-20k: min %.2f max %.2f deg (|d| should be ~90)" % (np.abs(d[m]).min(), np.abs(d[m]).max()), "sign", np.sign(d[m]).mean())
