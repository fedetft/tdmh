clear
clc
figure(1);
clf

//ps:mu/(1+s*T);
//pz:ratsimp(subst(s=(z-1)/z/Ts,ps));
//ratsimp(solve(rz*pz/(1+rz*pz)=(1-a)/z/(z-a),rz));

a=0.9985;
F=(1-a)/%z/(%z-a);

t=1:6000;
u=ones(t);
u(1)=0;

cv=dsimul(syslin('d',tf2ss(F)),u);
plot(t,cv,'k');

printf("Response time to 99%% = %f (minutes)",find(cv>.99)(1)/60);
