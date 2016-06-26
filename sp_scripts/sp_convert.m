% Function:  sp_convert
%
% Purpose:  Stewart platform - data input.
%           Create the binary data file for platform control from arbitrary platorm motion
%
% Invocation:  [L, Lvec] = sp_convert(x, y, z, roll, pitch, yaw, timestep, loop, loopend, filename)
%
% Argument         I/O  Description
% -------------    ---  ---------------------------------------------------
% x                 I   Position (in) of the x axis of the platform center in base frame (N cases)
% y                 I   Position (in) of the y axis of the platform center in base frame (N cases)
% z                 I   Position (in) of the z axis of the platform center in base frame (N cases)
% roll              I   Rotation (deg) about the x axis of the platform (N cases)
% pitch             I   Rotation (deg) about the y axis of the platform (N cases)
% yaw               I   Rotation (deg) about the z axis of the platform (N cases)
% timestep          I   Timestep (s) of the motion data
% loop              I   Datapoint to loop back if desired
% loopend           I   Datapoint to begin looping at
% filename          I   Name to give the binary file created
% L(6,N)            O   Lengths (in) of the 6 pistons for each case
% Lvec(3,6,N)       O   Piston vectors (cm) in base frame from base attachment
%                       points to platform attachment points
% ier               O   Error flag (0=ok, 1=bad input)
%
% External References:
% Function Name         Purpose
% -------------         ---------------------------------------------------
% sp_parameters         Script defining physical platform parameters
%
% Global References:
% Parameters            Description
% ----------            ---------------------------------------------------
% B_attach(3,6)         Positions (cm) in base frame where pistons attach to base
% P_attach(3,6)         Positions (cm) in platform frame where pistons attach
%                       to platform
%
% Copyright (C) 2015, Sparlak Consulting, LLC.  All Rights Reserved.

% Development History:
% Name           Date        Description of Change
% -------------  ----------  ----------------------------------------------
% G. Langan      04/03/2015  Comments added
% G. Langan      04/04/2015  Input parameter units changed

function  [L, Lvec, ier] = sp_convert(x,y,z,roll,pitch,yaw, timestep, loop, loopend, filename)

% Begin dubious method of finding quaternions, copied from the sp_lengths
% script
angle = [transpose(roll*(pi/180));transpose(pitch*(pi/180));transpose(yaw*(pi/180))];
q(4,:) = cos(0.5*sqrt(sum(angle(:,:).*angle(:,:),1)));
for i=1:3
    if std(angle(i,:))~=0
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

% get the rotation matrix and the pistons lengths
A = qtoa([q(1,:); q(2,:); q(3,:); q(4,:)]);
%[L,Lvec]=sp_inverse([transpose(x*2.54);transpose(y*2.54);(30*2.54) + transpose(z*2.54)],A);
[L,Lvec]=sp_inverse([transpose(x*2.54);transpose(y*2.54);(28.6922*2.54) + transpose(z*2.54)],A);

% Convert the lengths to inches and zero
L    = L/2.54;
L    = L - 41.75;
%L    = L - 42;

%{
% Check to make sure the results are within bounds
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
%}

% Find the number of datapoints and put the length data into a single column the
% control program can easily process
N = size(x,1);
data = zeros(1,N*6+3);
for i=1:N
    for j=1:6
        data((i-1)*6+j+4)=L(j,i);
    end
end

% Add the control variables to the beginning of the data file
data(1) = N;
data(2) = 1000*timestep;
data(3) = loopend;
data(4) = loop;
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