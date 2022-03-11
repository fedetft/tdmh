clear
clc
clf

mu = 1250;
T  = 200;
Ts = 1;
a  = 0.9985;
K  = 1.4e-6;

t = 1:6000;
u = ones(t);
u(1) = 0;

//P = mu / (1 + T * %s);
//R = K * (1 + T * %s) / %s;
//L = R * P;

Pz = (mu * Ts * %z) / ((Ts + T) * %z - T);
Rz = ( (1-a)*(Ts+T)*%z - T*(1-a) ) / ( mu*Ts*%z^3 - a*mu*Ts*%z^2 - mu*Ts*(1-a)*%z );
L = Rz * Pz

F = L / (1 + L); // = (1 - a) / (%z * (%z - a))

f = get("current_figure")
f.figure_size = [1200,1200];
                       
// Process plot        
subplot(411);          
xlabel("t", "fontsize", 4);
ylabel("Pz", "fontsize", 4);
ax = get("current_axes");
ax.font_size = 4;
//plot(t, csim(u, t, syslin('c', P)));
plot(t, dsimul(tf2ss(Pz), u));

// Step response simulation
//stepresponse = csim(u, t, syslin('c', F));
stepresponse = dsimul(tf2ss(F), u);
subplot(412);
xlabel("t", "fontsize", 4);
ylabel("Step response", "fontsize", 4);
ax = get("current_axes");
ax.font_size = 4;
plot(t, stepresponse);

printf("\nResponse time to 99%% = %f (minutes) \n\n", find(stepresponse>0.99)(1) / 60);

printf("Simulating process controller : \n")

y = zeros(t);
u = zeros(t);
e = zeros(t);
l = length(t);

setPoint = 500;

u1 = 0;
u2 = 0;
e1 = 0;
e2 = 0;
e3 = 0;
startTemp = 20; // start temperature
y1 = startTemp;
for i = 1:length(t)
    
    e(i) = setPoint - y1;
    u(i) = a*u1 + (1-a)*u2 + (1-a)*((T+Ts)/(mu*Ts))*e2 - (1-a)*(T/(mu*Ts))*e3;
    y(i) = (T / (T + Ts)) * y1 + mu * (Ts / (T + Ts)) * u(i);
    
    //printf("Iteration %d: e1=%f e2=%f e3=%f u1=%f u2=%f y1=%f\n", i, e1, e2, e3, u1, u2, y1);
    if i == 1 || pmodulo(i, 100) == 0 then
        printf("Iteration %d: e=%f u=%f y=%f\n", i, e(i), u(i), y(i));
    end
    
    u2 = u1;
    u1 = max(0, min(1, u(i)));
    e3 = e2;
    e2 = e1;
    e1 = e(i);
    y1 = y(i);
end

subplot(413);
xlabel("t", "fontsize", 4);
ylabel("Y", "fontsize", 4);
ax = get("current_axes");
ax.font_size = 4;
//plot(t(3:l), y(3:l));
plot(t, y);
subplot(414);
xlabel("t", "fontsize", 4);
ylabel("U", "fontsize", 4);
ax = get("current_axes");
ax.font_size = 4;
//plot(t(3:l), u(3:l));
plot(t, u);
