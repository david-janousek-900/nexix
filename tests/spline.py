"""Retained spline against independently solved dense boundary-value systems."""
from v7_command import Command

def solve(matrix,rhs):
    a=[list(row)+[value] for row,value in zip(matrix,rhs)];n=len(a)
    for i in range(n):
        k=max(range(i,n),key=lambda j:abs(a[j][i]));a[i],a[k]=a[k],a[i]
        scale=a[i][i];assert abs(scale)>1e-12
        a[i]=[v/scale for v in a[i]]
        for j in range(n):
            if j!=i:
                scale=a[j][i];a[j]=[x-scale*y for x,y in zip(a[j],a[i])]
    return [row[-1] for row in a]

def derivative(points,k=0,periodic=False):
    n=len(points);h=[points[i+1][0]-points[i][0] for i in range(n-1)]
    if periodic:
        m=n-1;a=[[0.0]*m for _ in range(m)];r=[]
        for i in range(m):
            prev=(i-1)%m;nxt=(i+1)%m
            a[i][i]+=2*(h[prev]+h[i]);a[i][prev]+=h[prev];a[i][nxt]+=h[i]
            r.append(6*((points[i+1][1]-points[i][1])/h[i]-(points[prev+1][1]-points[prev][1])/h[prev]))
        values=solve(a,r);return values+[values[0]]
    a=[[0.0]*n for _ in range(n)];r=[0.0]*n;a[0][0]=a[-1][-1]=1;a[0][1]=-k;a[-1][-2]=-k
    for i in range(1,n-1):
        a[i][i-1]=h[i-1];a[i][i]=2*(h[i-1]+h[i]);a[i][i+1]=h[i]
        r[i]=6*((points[i+1][1]-points[i][1])/h[i]-(points[i][1]-points[i-1][1])/h[i-1])
    return solve(a,r)

def expected(points,curvature,x):
    i=next((i for i in range(len(points)-1) if points[i][0]<=x<=points[i+1][0]),len(points)-2)
    h=points[i+1][0]-points[i][0];a=(points[i+1][0]-x)/h;b=1-a
    return a*points[i][1]+b*points[i+1][1]+((a*a*a-a)*curvature[i]+(b*b*b-b)*curvature[i+1])*h*h/6

CASES=[([],[(0,0),(1,1),(2,0)],0,False),(['-k','1'],[(0,0),(1,1),(2,4)],1,False),(['-p'],[(0,0),(1,1),(2,0)],0,True),([],[(0,2),(.5,1),(1.5,3),(2,0),(4,2)],0,False)]
if __name__=='__main__':
    c=Command('spline')
    for flags,points,k,periodic in CASES:
        data=''.join(f'{x} {y}\n' for x,y in points);r=c.run([*flags,'-n','24'],data=data)
        assert r.returncode==0,(r.stdout,r.stderr)
        curvature=derivative(points,k,periodic)
        for line in r.stdout.splitlines():
            x,y=map(float,line.split());want=expected(points,curvature,x)
            assert abs(y-want)<2e-5,(flags,x,y,want)
    c.check([],data='0 1\n1 3\n',stdout='0.000000 1.000000\n1.000000 3.000000\n')
    r=c.run(['-a','2','-n','8'],data='1\n5\n9\n');assert r.returncode==0
    for line in r.stdout.splitlines():
        x,y=map(float,line.split());assert abs(y-(2*x+1))<1e-5
    c.run(['-p'],data=''.join(f'{i} 0\n' for i in range(1000)))
    for args,data,message in [(['-n','0'],'','invalid point count'),([], '1 nan\n','invalid number'),([], '0 1\n2\n','missing ordinate'),([], '9'*200,'number too long'),([], ''.join(f'{i} 0\n' for i in range(1001)),'too many input points')]:
        c.check(args,data=data,stdout='',stderr='spline: '+message+'\n',status=1)
    print('PASS: spline independent dense natural/general/periodic systems, automatic abscissas, 1000-point bound and malformed numbers (ASan/UBSan)')
