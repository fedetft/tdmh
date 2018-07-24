clear; clc

function plot_data(filename,c)
    a=fscanfMat(filename);
    numnodes=a(:,1);
    convergence=a(:,4);
    plot(numnodes,convergence,c);
//    plot2d('nl',numnodes,convergence);
endfunction

figure(1); clf
//plot_data('results_scalability_nodeformation/rhex_8.txt','.-r');
plot_data('results_scalability_nodeformation/rhex_16.txt','.-r');
plot_data('results_scalability_nodeformation/rhex_32.txt','.-r');
plot_data('results_scalability_nodeformation/rhex_64.txt','.-r');
plot_data('results_scalability_nodeformation/rhex_128.txt','.-r');

figure(2); clf
//plot_data('results_scalability_nodeformation/hex_8.txt','.-k');
plot_data('results_scalability_nodeformation/hex_16.txt','.-k');
plot_data('results_scalability_nodeformation/hex_32.txt','.-k');
plot_data('results_scalability_nodeformation/hex_64.txt','.-k');
plot_data('results_scalability_nodeformation/hex_128.txt','.-k');
