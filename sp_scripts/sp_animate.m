function  handle = sp_animate(Lvec)

N = size(Lvec,3);
step = round(N/30);
for i=1:step:N
    sp_display(Lvec(:,:,i));
end