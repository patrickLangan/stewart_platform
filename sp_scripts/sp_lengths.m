function  [L, Lvec, Pc, angle, q, A, ier] = sp_lengths(amp, freq, avg, phase, timestep, filename)

% Initialization
L    = [];
Lvec = [];
ier  = 0;

[ma,na] = size(amp);
[mf,nf] = size(freq);
[mv,nv] = size(avg);

% Eventually put this in a function and check all of the input variables
if ma~=6
   fprintf('\n*** ERROR. Row dimension of amp must be 6.\n\n');
   ier = 1;
   return
end

if na~=1
    fprintf('\n*** ERROR. Column dimension of amp must be 1.\n\n');
    ier = 1;
    return
end


amp(1:3) = amp(1:3)*2.54;
amp(4:6) = amp(4:6)*(pi/180);
avg(1:3) = avg(1:3)*2.54;
avg(4:6) = avg(4:6)*(pi/180);
phase = phase*(pi/180);

loop = round(0.5/(freq*timestep) + (phase/(2*pi))/(freq*timestep));
loopend = round(1/(freq*timestep) + loop);
N = round(0.5/(freq*timestep)) + loopend;

Pc = zeros(3,max(N));
angle = zeros(3,max(N));
for j=1:3
    if phase(j)<max(phase)
        loopend(j) = loopend(j) + round(1/(freq*timestep));
        N(j) = N(j) + round(1/(freq*timestep));
    end
    for i=1:loop(j)
        if amp(j)>0
            if j~=3
                Pc(j,i) = 0.5*(-avg(j) + amp(j))*sin((timestep*(i-1)*pi*2*(1/(2*loop(j)*timestep))) + 0.5*pi) + 0.5*(avg(j)-amp(j));
            else
                Pc(3,i) = 0.5*((19*2.54)-(avg(3)-amp(3)))*sin((timestep*(i-1)*pi*2*(1/(2*loop(j)*timestep))) + 0.5*pi) + (19*2.54) - 0.5*((19*2.54)-(avg(3)-amp(3)));
            end
        else
            Pc(j,i) = avg(j);
        end
    end
    for i=loop(j)+1:loopend(j)
        Pc(j,i) = amp(j)*sin((timestep*(i-1)*pi*2*freq) - phase(j) + 0.5*pi) + avg(j);
    end
    for i=loopend(j)+1:N(j)
        if amp(j)>0
            if j~=3
                Pc(j,i) = 0.5*(-avg(j) + amp(j))*sin((timestep*(i-1)*pi*2*freq) + 0.5*pi - phase(j)) + 0.5*(avg(j) - amp(j));
            else
                Pc(3,i) = 0.5*((19*2.54)-(avg(3)-amp(3)))*sin((timestep*(i-1)*pi*2*freq) + 0.5*pi - phase(j)) + (19*2.54) - 0.5*((19*2.54)-(avg(3)-amp(3)));
            end
        else
            Pc(j,i) = avg(j);
        end
    end
    for i=N(j):max(N)
        Pc(j,i) = Pc(j,N(j));
    end
end
for j=4:6
    if phase(j)<max(phase)
        loopend(j) = loopend(j) + round(1/(freq*timestep));
        N(j) = N(j) + round(1/(freq*timestep));
    end
    for i=1:loop(j)
        if amp(j)>0
            angle(j-3,i) = 0.5*(-avg(j) + amp(j))*sin((timestep*(i-1)*pi*2*(1/(2*loop(j)*timestep))) + 0.5*pi) + 0.5*(avg(j)-amp(j));
        else
            angle(j-3,i) = 0;
        end
    end
    for i=loop(j)+1:loopend(j)
        angle(j-3,i) = amp(j)*sin((timestep*(i-1)*pi*2*freq) - phase(j) + 0.5*pi) + avg(j);
    end
    for i=loopend(j)+1:N(j)
        if amp(j)>0
            angle(j-3,i) = 0.5*(-avg(j) + amp(j))*sin((timestep*(i-1)*pi*2*freq) + 0.5*pi - phase(j)) + 0.5*(avg(j) - amp(j));
        else
            angle(j-3,i) = 0;
        end
    end
    for i=N(j)+1:max(N)
            angle(j-3,i) = angle(j-3,N(j));
    end
end 

Pc(3,:) = Pc(3,:) + (30 * 2.54);

%q = zeros(4,N);
q(4,:) = cos(0.5*sqrt(sum(angle(:,:).*angle(:,:),1)));
for i=1:3
    if amp(i+3)~=0
        q(i,:) = sin(0.5*sqrt(sum(angle(:,:).*angle(:,:),1))).*(angle(i,:)./(sqrt(sum(angle(:,:).*angle(:,:),1))));
        for j=1:3
            if isnan(q(i,j))
                q(i,j) = 0;
            end
        end
    else
        q(i,:) = q(4,:)*0;
    end
end
q(1,1)=0;
A = qtoa([q(1,:); q(2,:); q(3,:); q(4,:)]);
[L, Lvec] = sp_inverse(Pc, A);

L    = L/2.54;
L    = L - 42;

if min(min(transpose(L)))<0
   fprintf('\n*** ERROR. One or more of the pistons lengths goes below zero.\n\n');
   min(min(transpose(L)))
   ier = 1;
   return
end
if max(max(transpose(L)))>28
   fprintf('\n*** ERROR. One or more of the pistons lengths exceeds the maximum of 28 inches.\n\n');
   max(max(transpose(L)))
   ier = 1;
   return
end

LTemp = L;

L(1, :) = LTemp(5, :);
L(2, :) = LTemp(6, :);
L(3, :) = LTemp(1, :);
L(4, :) = LTemp(2, :);
L(5, :) = LTemp(3, :);
L(6, :) = LTemp(4, :);

hold on;
plot (L(1, :), 'r');
plot (L(2, :), 'g');
plot (L(3, :), 'b');
plot (L(4, :), 'y');
plot (L(5, :), 'm');
plot (L(6, :), 'c');
hold off;
legend ('1', '2', '3', '4', '5', '6', 'Location', 'northwest');

%{
L(1, :) = 12.5;
L(2, :) = 12.5;
L(3, :) = 12.5;
L(4, :) = 12.5;
L(5, :) = 12.5;
%L(6, :) = 12.5;

hold on;
plot (L(1, :));
plot (L(2, :));
plot (L(3, :));
plot (L(4, :));
plot (L(5, :));
plot (L(6, :));
hold off;
%}

data = zeros(1,max(N)*6+3);
for i=1:N
    for j=1:6
        data((i-1)*6+j+4)=L(j,i);
    end
end

data(1) = max(N);
data(2) = 1000*timestep;
data(3) = min(loopend);
data(4) = max(loop) + 1;
data    = single(data);

binaryFile = strcat (filename, '.bin');
fid=fopen(binaryFile,'w');
fwrite(fid,data,'single');
fclose(fid);

textFile = strcat (filename, '.csv');
fid = fopen (textFile, 'w');
fprintf (fid, 'stepNum: %f\n', data(1));
fprintf (fid, 'timeStep: %f\n', data(2));
fprintf (fid, 'loopEnd: %f\n', data(3));
fprintf (fid, 'loopStart: %f\n\n', data(4));
for i = 0:max(N) - 1
    for j = 0:4
        fprintf (fid, '%f, ', data((i * 6) + j + 5));
    end
    fprintf (fid, '%f\n', data((i * 6) + 10));
end
fclose (fid);