clear; clc

x=[2,4,8,16,32,64,128];

function convergence=get_convergence_times(filename,n,mark)
    a=fscanfMat(filename);
    ct=a(:,4);
    convergence=resize_matrix(ct',1,n,%nan);
    plot2d('ll',x,convergence,-mark);
endfunction

function print_convergence(convergence)
    printf("Convergence time [s] as a function of N_max=maximum number of\n");
    printf("node (TDMH-MAC configuration parameter) and N=actual numer of nodes\n")
    printf("\t\tN=2\tN=4\tN=8\tN=16\tN=32\tN=64\tN=128\n");
    [r,c]=size(hex);
    for i=1:r
        printf("N_max=%d ",x(length(x)+1-i));
        for j=1:c
            if isnan(convergence(i,j))==%F then
                printf("\t%3.1f",convergence(i,j));
            end
        end
        printf("\n");
    end
endfunction

figure(1); clf
hex=     get_convergence_times('results_scalability_networkformation/hex_128.txt',7,1);
hex=[hex;get_convergence_times('results_scalability_networkformation/hex_64.txt',7,2)];
hex=[hex;get_convergence_times('results_scalability_networkformation/hex_32.txt',7,3)];
hex=[hex;get_convergence_times('results_scalability_networkformation/hex_16.txt',7,4)];
hex=[hex;get_convergence_times('results_scalability_networkformation/hex_8.txt',7,5)];
title("Hexagon topology (best case ordering)");
xlabel("Number of nodes");
ylabel("Convergence time [s]");
legend(["N_{max}=128","N_{max}=64","N_{max}=32","N_{max}=16","N_{max}=8"],4);
printf("Hexagon topology (best case ordering)\n");
print_convergence(hex);

printf("\n\n\n");


figure(2); clf
rhex=      get_convergence_times('results_scalability_networkformation/rhex_128.txt',7,1);
rhex=[rhex;get_convergence_times('results_scalability_networkformation/rhex_64.txt',7,2)];
rhex=[rhex;get_convergence_times('results_scalability_networkformation/rhex_32.txt',7,3)];
rhex=[rhex;get_convergence_times('results_scalability_networkformation/rhex_16.txt',7,4)];
rhex=[rhex;get_convergence_times('results_scalability_networkformation/rhex_8.txt',7,5)];
title("Reverse hexagon topology (worst case ordering)");
xlabel("Number of nodes");
ylabel("Convergence time [s]");
legend(["N_{max}=128","N_{max}=64","N_{max}=32","N_{max}=16","N_{max}=8"],4);
printf("Reverse hexagon topology (worst case ordering)\n");
print_convergence(rhex);
