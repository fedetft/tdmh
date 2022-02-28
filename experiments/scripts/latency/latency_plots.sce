
//#####################################################
//#
//#                       TODO
//#
//# also plot latency average and variance if needed
//#####################################################

function plt(filename)
    a = fscanfMat(filename);
    //if length(a) > 0 then
        plot(a(:,1),a(:,2)/1000000); // nanoseconds to milliseconds
        xlabel("Packets number", "fontsize", 4);
        ylabel("Latency [ms]", "fontsize", 4);
        //PDF export
        if needToExport == "true" then
            scf(0);
            l = length(args);
            xs2pdf(0, outputFile);
            xs2pdf(gcf(), outputFile);
            exit();
        end
    //end
endfunction

args = sciargs()
inputFile = args(5);
needToExport = args(6);
if needToExport == "true" then
    outputFile = args(7);
end

t = gce();
t.font_size = 4;

f = get("current_figure")
f.figure_size = [1000,800];

//subplot(221);
plt(inputFile);

//title("Latency [ms]", "fontsize", 4);
//subplot(222);
//plt('latency_mean.txt');
//title("Average latency [ms]");
//xlabel("Packets number", "fontsize", 4);
//ylabel("Averahe latency [ms]", "fontsize", 4);
//subplot(223);
//plt('latency_stddev.txt');
//title("Latency standard deviation [ms]");
//xlabel("Packets number", "fontsize", 4);
//ylabel("Latency std. dev. [ms]", "fontsize", 4);
