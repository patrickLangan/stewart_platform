% Function:  sp_inverse
%
% Purpose:  Stewart platform - inverse problem.
%           Given position and attitude of platform, solve for 6 piston lengths.
%
% Invocation:  [L, Lvec] = sp_inverse(Pc, A);
%
% Argument         I/O  Description
% -------------    ---  ---------------------------------------------------
% Pc(3,N)           I   Position (cm) of platform center in base frame (N cases)
% A(3,3N) or (9,N)  I   Platform attitude (direction cosine matrices for the
%                       base frame-to-platform frame transformations for N cases)
%                      [Note, if Pc or A includes N cases and the other includes
%                       only one, then that one will be duplicated N times.]
% L(6,N)            O   Lengths (cm) of the 6 pistons for each case
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
% J. Sedlak      01/18/2015  Created


function  [L, Lvec, ier] = sp_inverse(Pc, A)

global  B_attach  P_attach

% Initialization
L    = [];
Lvec = [];
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
AT   = A([1,4,7,2,5,8,3,6,9],:);  %transpose of A in (9,:) format
for i=1:6
   v  = P_attach(:,i);
   vb = [AT(1,:)*v(1,1) + AT(4,:)*v(2,1) + AT(7,:)*v(3,1);...
         AT(2,:)*v(1,1) + AT(5,:)*v(2,1) + AT(8,:)*v(3,1);...
         AT(3,:)*v(1,1) + AT(6,:)*v(2,1) + AT(9,:)*v(3,1) ];
   Lvec(:,i,:) = vb + Pc - repmat(B_attach(:,i),1,N);
   L(i,:)      = sqrt(sum(Lvec(:,i,:).*Lvec(:,i,:),1));
end