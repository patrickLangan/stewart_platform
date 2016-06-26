% Function:  sp_forces
%
% Purpose:  Stewart platform - inverse problem.
%           Given position and attitude of platform, solve for 6 piston lengths and forces.
%
% Invocation:  [F, L] = sp_forces(Pc, A, a);
%
% Argument         I/O  Description
% -------------    ---  ---------------------------------------------------
% Pc(3,N)           I   Position (cm) of platform center in base frame (N cases)
% A(3,3N) or (9,N)  I   Platform attitude (direction cosine matrices for the
%                       base frame-to-platform frame transformations for N cases)
%                      [Note, if Pc or A includes N cases and the other includes
%                       only one, then that one will be duplicated N times.]
% a(6,N)            I   Acceleration (cm/s^2) of the platform in each DOF
% L(6,N)            O   Lengths (cm) of the 6 pistons for each case
% F(6,N)            O   Forces (N) of the 6 pistons for each case
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
% J. Sedlak      01/18/2015  Created
% G. Langan      02/9/2015   Force calculation added

function  [F, L, ier] = sp_forces(Pc, A, a)

%start with the sp_inverse script
global  B_attach  P_attach

% Initialization
F    = [];
ier  = 0;

[mp,np] = size(Pc);
[ma,na] = size(A);

if ma==3
   if mod(na,3)~=0
      fprintf('\n*** ERROR. Size of A is invalid.\n\n');
      ier = 1;
      return
   end
   N = na/3;
   A = reshape(A,9,N);
elseif ma==9
   N = na;
else
   fprintf('\n*** ERROR. Row dimension of A must be 3 or 9.\n\n');
   ier = 1;
   return
end

if mp~=3
   fprintf('\n*** ERROR. Row dimension of Pc must be 3.\n\n');
   ier = 1;
   return
end

if np~=N
   if ~(np==1 || N==1)
      fprintf('\n*** ERROR. Sizes of Pc and A are inconsistent.\n\n');
      ier = 1;
      return
   end
   if np==1
      Pc = repmat(Pc,1,N);
   elseif N==1
      N = np;
      A = repmat(A,1,N);
   end
end

% Read parameters file
sp_parameters;

% Compute vectors
% (The vb computation is a way to do N simultaneous matrix multiplications)
Lvec = zeros(3,6,N);
L    = zeros(3,N);
Marms = zeros(3,6,N);
AT   = A([1,4,7,2,5,8,3,6,9],:);  %transpose of A in (9,:) format
for i=1:6
   v  = P_attach(:,i);
   vb = [AT(1,:)*v(1,1) + AT(4,:)*v(2,1) + AT(7,:)*v(3,1);...
         AT(2,:)*v(1,1) + AT(5,:)*v(2,1) + AT(8,:)*v(3,1);...
         AT(3,:)*v(1,1) + AT(6,:)*v(2,1) + AT(9,:)*v(3,1) ];
   Lvec(:,i,:) = vb + Pc - repmat(B_attach(:,i),1,N);
   L(i,:)      = sqrt(sum(Lvec(:,i,:).*Lvec(:,i,:),1));
   Marms(:,i,:)  = vb;
end

% Force summation script

% Mass of the platform
%{
m1 = 60;
g = 981;
% Initialize the moment of inertia tensor
I = zeros(3,3);
% Roll moment of inertia about the x axis
I(1,1) = 20000;
% Pitch moment of inertia about the y axis
I(2,2) = 20000;
% Yaw moment of inertia about the z axis
I(3,3) = 38000;
%}

g = 981;

m_ring = 20.0;
m_point = 15.0 + 100.0 * 0.453592;
m1 = m_ring + m_point;

R = 24.0 * 0.0254 * 100; %cm

I = zeros(3,3);
I(1,1) = 0.5 * m_ring * R^2;
I(2,2) = 0.5 * m_ring * R^2;
I(3,3) = m_ring * R^2;

% Transform the angular accelerations into the base reference frame using the rotation matrix
ab = zeros(6,N);
for i=1:N
	av = a(:,i);
	ab(:,i) = [av(1,1);...
	           av(2,1);...
	           av(3,1);...
	           AT(1,i)*av(4,1) + AT(4,i)*av(5,1) + AT(7,i)*av(6,1);...
	           AT(2,i)*av(4,1) + AT(5,i)*av(5,1) + AT(8,i)*av(6,1);...
	           AT(3,i)*av(4,1) + AT(6,i)*av(5,1) + AT(9,i)*av(6,1) ];
end

Ib = zeros(3,N);
for i=1:3
   for j=0:3:6
      for k=0:3:6
         Ib(i,:) = Ib(i,:) + AT(i+j,:).*I((j/3)+1,(k/3)+1).*AT(i+k,:);
      end
   end
end

% Unitize Lvec
Uvec = Lvec./repmat(sqrt(sum(Lvec.*Lvec,1)),3,1);
M = zeros(3,6,N);
for i=1:6
   M(:,i,:) = [Uvec(3,i,:).*Marms(2,i,:) - Uvec(2,i,:).*Marms(3,i,:);...
               Uvec(1,i,:).*Marms(3,i,:) - Uvec(3,i,:).*Marms(1,i,:);...
               -Uvec(1,i,:).*Marms(2,i,:) + Uvec(2,i,:).*Marms(1,i,:) ];
end

% Create the simultaneous equations in the form F * A = b
A = [Uvec; M];
b = [ m1*ab(1,:);
      m1*ab(2,:);
      m1*(ab(3,:) + g);
      Ib(1,:).*ab(4,:);
      Ib(2,:).*ab(5,:);
      Ib(3,:).*ab(6,:) ];

% Solve the simultaneous equations for F
F = zeros(6,N);
for i=1:N
	F(:,i) = 100*A(:,:,i)\b(:,i);
end
