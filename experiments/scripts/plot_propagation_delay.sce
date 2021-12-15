a = fscanfMat("propagation_delay.log");
cumulated = a(:,1);
roundtrip = a(:,2);
filtered = a(:,3);

figure(1);
clf;
subplot(311);
plot(cumulated);
subplot(312);
plot(roundtrip);
subplot(313);
plot(filtered);


printf("Media round trip: %f\n", nanmean(roundtrip));
printf("Media filtered: %f\n", nanmean(filtered));
printf("Deviazione standard round trip: %f\n", nanstdev(roundtrip));
printf("Deviazione standard filtered: %f\n", nanstdev(filtered));
