function T = vox2ras_tkregmtx(voldim, voxres)
% T = vox2ras_tkregmtx(voldim, voxres)
%   voldim = [ncols  nrows  nslices ...]
%   volres = [colres rowres sliceres ...]
%
% Computes the 0-based vox2ras transform of a volume
% compatible with the registration matrix produced
% by tkregister. May work with MINC xfm.
%

T = [];

if(nargin ~= 2)
  fprintf('T = vox2ras_tkregmtx(voldim, voxres)\n');
  return;
end

T = zeros(4,4);
T(4,4) = 1;

T(1,1) = -voxres(1);
T(1,4) = voxres(1)*voldim(1)/2;

T(2,3) = voxres(3);
T(2,4) = -voxres(3)*voldim(3)/2;


T(3,2) = -voxres(2);
T(3,4) = voxres(2)*voldim(2)/2;


return;
