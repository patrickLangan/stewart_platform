% Function:  sp_solve
%
% Purpose:  Stewart platform - forward problem.
%           Given 6 piston lengths, solve for position and attitude of platform.
%
% Invocation:  [Pc, A, ier] = sp_solve(Lmeas);
%
% Argument         I/O  Description
% -------------    ---  ---------------------------------------------------
% Lmeas(6,1)        I   Measured lengths (cm) of the 6 pistons
% Pc(3,1)           O   Position (cm) of platform center in base frame
% A(3,3)            O   Platform attitude (direction cosine matrix for the
%                       base frame-to-platform frame transformation)
% ier               O   Error flag (0=ok, 1=bad input, 2=failure to converge)
%
% External References:
% Function Name         Purpose
% -------------         ---------------------------------------------------
% sp_parameters         Script defining physical platform parameters
% quest                 Optimal quaternion estimator
% qtoa                  Converts quaternions to direction-cosine matrices
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

function  [Pc, A, ier] = sp_solve(Lmeas)

global  B_attach  P_attach

% Initialization
Pc  = [];
A   = [];
ier = 0;

Lmeas = Lmeas(:);                %ensure Lmeas is a column vector
if length(Lmeas)~=6
   fprintf('\n*** ERROR. Input Lmeas must be of length 6.\n\n');
   ier = 1;
   return
end

nmax   = 1e9;                    %maximum number of iterations
angtol = 0.1*pi/(180*3600);      %angle tolerance in radians
postol = 0.0001;                 %position tolerance in cm
w      = ones(length(Lmeas),1);  %equal weights for all observations in QUEST

% Read parameters file
sp_parameters;

% Initial guess for Pc and A
b    = (sum(Lmeas)/length(Lmeas))*sqrt(3)/2;  %rough estimate of platform height
Pold = [0; 0; b];
Aold = eye(3);

% Iterate on estimation of Pc and A
for n=1:nmax
   % Use current guess for Pc and A to get Lvec estimates
   [L, Lvec] = sp_inverse(Pold, Aold);
   
   % Change Lvec to the measured lengths
   Uvec = Lvec./repmat(sqrt(sum(Lvec.*Lvec,1)),3,1);  %unitize Lvec
   Lvec = repmat(Lmeas',3,1).*Uvec;
   
   % Vectors from platform center to piston attachment points on platform
   % expressed in base frame
   ref = B_attach + Lvec - repmat(Pold, 1, 6);
   
   % New estimate for Pc
   Pc = sum(Lvec,2)/size(Lvec,2);
   
   % New estimate for A
   q = quest(P_attach, ref, w);
   A = qtoa(q);
   
   % Check for convergence
   d    = A*Aold';
   dang = max(abs([d(1,2), d(1,3), d(2,1), d(2,3), d(3,1), d(3,2)]));
   dpos = max(abs(Pc-Pold));
   
   if dang<angtol && dpos<postol
      break
   elseif n==nmax
      fprintf('\n*** Did NOT converge in %i iterations.\n\n',n);
      ier = 2;
      return
   end
   
   Pold = Pc;
   Aold = A;
end

fprintf('\n*** Converged in %i iterations.\n\n',n);


