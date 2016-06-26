% Function:  sp_display
%
% Purpose:  3-D plot of Stewart platform
%
% Invocation:  handle = sp_display(Lvec);
%
% Argument         I/O  Description
% -------------    ---  ---------------------------------------------------
% Lvec(3,6)         I   Piston vectors (cm) in base frame from base attachment
%                       points to platform attachment points
% handle            O   Figure handle
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
%
% Copyright (C) 2015, Sparlak Consulting, LLC.  All Rights Reserved.

% Development History:
% Name           Date        Description of Change
% -------------  ----------  ----------------------------------------------
% J. Sedlak      01/18/2015  Created

function  handle = sp_display(Lvec)

global  B_attach

handle = figure;

XB = [B_attach(1,:), B_attach(1,1)];
YB = [B_attach(2,:), B_attach(2,1)];
ZB = [B_attach(3,:), B_attach(3,1)];

XP = [B_attach(1,:), B_attach(1,1)] + [Lvec(1,:), Lvec(1,1)];
YP = [B_attach(2,:), B_attach(2,1)] + [Lvec(2,:), Lvec(2,1)];
ZP = [B_attach(3,:), B_attach(3,1)] + [Lvec(3,:), Lvec(3,1)];

LX = [XB(1:6); XP(1:6)];
LY = [YB(1:6); YP(1:6)];
LZ = [ZB(1:6); ZP(1:6)];

% Choose axis limits
x1 = min(XB);
x2 = max(XB);
y1 = min(YB);
y2 = max(YB);
z1 = 0;
z2 = max(ZP);
x1 = 10*(fix(abs(x1)/10)+1)*sign(x1);
x2 = 10*(fix(abs(x2)/10)+1)*sign(x2);
y1 = 10*(fix(abs(y1)/10)+1)*sign(y1);
y2 = 10*(fix(abs(y2)/10)+1)*sign(y2);
z2 = 10*(fix(abs(z2)/10)+1)*sign(z2);

% Plot and fill the base vertices
plot3(XB,YB,ZB,'k','linewidth',2);
xlabel('X  (cm)');
ylabel('Y  (cm)');
zlabel('Z  (cm)');
axis([x1 x2 y1 y2 z1 z2])
grid
hold on
patch(XB,YB,ZB,[.2,.2,.2],'FaceAlpha',0.7);

% Plot and fill the platform vertices
plot3(XP,YP,ZP,'color','k','linewidth',2);
patch(XP,YP,ZP,[.2,.2,.2],'FaceAlpha',0.96);

% Plot the support pistons
plot3(LX,LY,LZ,'color','b','linewidth',3);
hold off


% Eventually want to use this method to animate the display given large set of Lvec
% (Example from Matlab Help Doc)
% c = -pi:.04:pi;
% cx = cos(c);
% cy = -sin(c);
% hf = figure('color','white');
% axis off, axis equal
% line(cx, cy, 'color', [.4 .4 .8],'LineWidth',3);
% title('See Pythagoras Run!','Color',[.6 0 0])
% hold on
% x = [-1 0 1 -1];
% y =   [0 0 0 0];
% ht = area(x,y,'facecolor',[.6 0 0])
% set(ht,'XDataSource','x')
% set(ht,'YDataSource','y')
% for j = 1:length(c)
%     x(2) = cx(j);
%     y(2) = cy(j);
%     refreshdata(hf,'caller')
%     drawnow
% end

