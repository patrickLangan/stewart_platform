function  sp_solver (c1, c2, c3, c4, c5, c6, fileName)
%input and output is in inches and degrees

%Check that the input arrays are equal length
cylinderLengths = [c1, c2, c3, c4, c5, c6];

%fprintf ('x, y, z, roll, pitch, yaw\n');
file = fopen (fileName, 'w');
fprintf (file, 'x, y, z, roll, pitch, yaw\n');

%Loop through input cylinder lengths and convert their direction-cosine matrices to angles
for i = 1:length (c1)
    %Run sp_solve (hiding printf output, unless there's an error)
    [stdin, Pc, A, ier] = evalc ('sp_solve ((cylinderLengths(i, :) + 41.75) * 2.54);');
    if (ier ~= 0)
        fprintf ('%s\n', stdin);
    else
        %{
        %Convert direction-cosine matrix to quaternion
        qw = sqrt (1.0 + A(1, 1) + A(2, 2) + A(3, 3)) / 2.0;
        qx = (A(3, 2) - A(2, 3)) / (4.0 * qw);
        qy = (A(1, 3) - A(3, 1)) / (4.0 * qw);
        qz = (A(2, 1) - A(1, 2)) / (4.0 * qw);

        %Convert quaternion to angles with modified quat2angle script (changed line 57)
        [yaw, pitch, roll] = quat2angle2 ([qw, qx, qy, qz]);
        roll = roll * (180.0 / pi);
        pitch = pitch * (180.0 / pi);
        yaw = yaw * (180.0 / pi);
        %}
        
        %%{
        [roll, pitch, yaw] = dcm2angle (A, 'XYZ');
        roll = roll * (180.0 / pi);
        pitch = pitch * (180.0 / pi);
        yaw = yaw * (180.0 / pi);
        %}
    end

    x = Pc(1) / 2.54;
    y = Pc(2) / 2.54;
    z = Pc(3) / 2.54;
    
    %fprintf ('%f, %f, %f, %f, %f, %f\n', x, y, z, roll, pitch, yaw);
    fprintf (file, '%f, %f, %f, %f, %f, %f\n', x, y, z, roll, pitch, yaw);
end

fclose (file);

end
