
function plt(filename)
    a=fscanfMat(filename);
    plot(a(:,1),a(:,2));
endfunction

subplot(221);
plt('heapFree.txt');
title("Free heap (master node) [B]");
subplot(222);
plt('macStack.txt');
title("Mac stack used (master node) [B]");
subplot(223);
plt('logBytes.txt');
title("Log buffer bytes used (master node) [B]");
subplot(224);
plt('schedStack.txt');
title("Scheduler stack used (master node) [B]");
