clear
clc
clf

args = sciargs();
inputFile = args(5);

f = get("current_figure")
f.figure_size = [1000,800];

a = fscanfMat(inputFile);

subplot(211);    
plot(a(:,1),a(:,2));
ax = get("current_axes");
ax.font_size = 4;
xlabel("Time [s]", "fontsize", 4);
ylabel("Temperature [°C]", "fontsize", 4);

subplot(212);  
plot(a(:,1),a(:,3)*100/60000); // show control action % (from 0 to 60000)
ax = get("current_axes");
ax.font_size = 4;
xlabel("Time [s]", "fontsize", 4);
ylabel("Control action [%]", "fontsize", 4);