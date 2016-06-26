% Script:  sp_parameters
%
% Purpose:  Defines globals for physical parameters for Stewart platform.
%
% External References:
% Function Name         Purpose
% -------------         ---------------------------------------------------
% none
%
% Global References:
% Parameters            Description
% ----------            ---------------------------------------------------
% B_attach(3,6)         Positions (cm) in base frame where pistons attach to base
% P_attach(3,6)         Positions (cm) in platform frame where pistons attach
%                       to platform
% Piston_L_init(1,6)    Unextended lengths (cm) of pistons
%
% Copyright (C) 2015, Sparlak Consulting, LLC.  All Rights Reserved.

% Development History:
% Name           Date        Description of Change
% -------------  ----------  ----------------------------------------------
% J. Sedlak      01/18/2015  Created

global  B_attach  P_attach  Piston_L_init

evalin('base', 'global  B_attach  P_attach  Piston_L_init')

B = 175.895;
d1 = 21.971 / B * 0.5;
B_attach(:,1) = B*[ 1/2+d1*sin(pi/6);	 1/(2*sqrt(3))-d1*cos(pi/6);    0];
B_attach(:,2) = B*[ 1/2-d1*sin(pi/6);	 1/(2*sqrt(3))+d1*cos(pi/6);	0];
B_attach(:,3) = B*[-1/2+d1*sin(pi/6);	 1/(2*sqrt(3))+d1*cos(pi/6);	0];
B_attach(:,4) = B*[-1/2-d1*sin(pi/6);	 1/(2*sqrt(3))-d1*cos(pi/6);	0];
B_attach(:,5) = B*[ 0.0-d1;             -1/sqrt(3);                     0];
B_attach(:,6) = B*[ 0.0+d1;             -1/sqrt(3);                     0];

P = 101.44;
d2 = 10.879 / P * 0.5;
P_attach(:,6) = P*[ 1/2-d2*sin(pi/6);	-1/(2*sqrt(3))-d2*cos(pi/6);	0];
P_attach(:,1) = P*[ 1/2+d2*sin(pi/6);	-1/(2*sqrt(3))+d2*cos(pi/6);	0];
P_attach(:,2) = P*[ 0.0+d2;              1/sqrt(3);                     0];
P_attach(:,3) = P*[ 0.0-d2;              1/sqrt(3);                     0];
P_attach(:,4) = P*[-1/2-d2*sin(pi/6);	-1/(2*sqrt(3))+d2*cos(pi/6);	0];
P_attach(:,5) = P*[-1/2+d2*sin(pi/6);	-1/(2*sqrt(3))-d2*cos(pi/6);	0];

Piston_L_init(1,:) = [60, 60, 60, 60, 60, 60];  %not currently used

