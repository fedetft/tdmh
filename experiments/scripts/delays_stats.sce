
function plt(filename)
    a=fscanfMat(filename);
    plot(a(:,1),a(:,2));
endfunction

subplot(221);
plt('delays.txt');
title("Delays [ns]");
subplot(222);
plt('delays_mean.txt');
title("Mean delay [ns]");
subplot(223);
plt('delays_stddev.txt');
title("Delays standard deviation [ns]");
