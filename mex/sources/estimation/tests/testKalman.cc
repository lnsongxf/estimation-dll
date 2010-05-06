/*
 * Copyright (C) 2010 Dynare Team
 *
 * This file is part of Dynare.
 *
 * Dynare is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Dynare is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Dynare.  If not, see <http://www.gnu.org/licenses/>.
 */

// Test  for InitializeKalmanFilter
// Uses fs2000k2e.mod and its ..._dynamic.mexw32

#include "KalmanFilter.hh"

int
main(int argc, char **argv)
{
  if (argc < 2)
    {
      std::cerr << argv[0] << ": please provide as argument the name of the dynamic DLL generated from fs2000k2.mod (typically fs2000k2_dynamic.mex*)" << std::endl;
      exit(EXIT_FAILURE);
    }

  std::string modName = argv[1];
  const int npar = 7; //(int)mxGetM(mxFldp);
  const size_t n_endo = 15, n_exo = 2;
  std::vector<size_t> zeta_fwrd_arg;
  std::vector<size_t> zeta_back_arg;
  std::vector<size_t> zeta_mixed_arg;
  std::vector<size_t> zeta_static_arg;
  //std::vector<size_t>
  double qz_criterium = 1.000001; //1.0+1.0e-9;
  Vector
  steadyState(n_endo), deepParams(npar);

  double dYSparams [] = {
    1.000199998312523,
    0.993250551764778,
    1.006996670195112,
    1,
    2.718562165733039,
    1.007250753636589,
    18.982191739915155,
    0.860847884886309,
    0.316729149714572,
    0.861047883198832,
    1.00853622757204,
    0.991734328394345,
    1.355876776121869,
    1.00853622757204,
    0.992853374047708
  };

  double vcov[] = {
    0.001256631601,	0.0,
    0.0,	0.000078535044
  };

  Matrix
  ll_incidence(3, n_endo); // leads and lags indices
  double inllincidence[] = {
    1,   5,  0,
    2,   6,  20,
    0,   7,  21,
    0,   8,   0,
    0,   9,   0,
    0,  10,   0,
    3,  11,   0,
    0,  12,   0,
    0,  13,   0,
    0,  14,   0,
    0,  15,   0,
    0,  16,   0,
    4,  17,   0,
    0,  18,   0,
    0,  19,  22,
  };
  MatrixView
  llincidence(inllincidence, 3, n_endo, 3); // leads and lags indices
  ll_incidence = llincidence;

  double dparams[] = {
    0.3560,
    0.9930,
    0.0085,
    1.0002,
    0.1290,
    0.6500,
    0.0100
  };

  VectorView
  modParamsVW(dparams, npar, 1);
  deepParams = modParamsVW;
  VectorView
  steadyStateVW(dYSparams, n_endo, 1);
  steadyState = steadyStateVW;
  std::cout << "Vector deepParams: " << std::endl << deepParams << std::endl;
  std::cout << "MatrixVw llincidence: " << std::endl << llincidence << std::endl;
  std::cout << "Matrix ll_incidence: " << std::endl << ll_incidence << std::endl;
  std::cout << "Vector steadyState: " << std::endl << steadyState << std::endl;

  // Set zeta vectors [0:(n-1)] from Matlab indices [1:n] so that:
  // order_var = [ stat_var(:); pred_var(:); both_var(:); fwrd_var(:)];
  size_t statc[] = { 4, 5, 6, 8, 9, 10, 11, 12, 14};
  size_t back[] = {1, 7, 13};
  size_t both[] = {2};
  size_t fwd[] = { 3, 15};
  for (int i = 0; i < 9; ++i)
    zeta_static_arg.push_back(statc[i]-1);
  for (int i = 0; i < 3; ++i)
    zeta_back_arg.push_back(back[i]-1);
  for (int i = 0; i < 1; ++i)
    zeta_mixed_arg.push_back(both[i]-1);
  for (int i = 0; i < 2; ++i)
    zeta_fwrd_arg.push_back(fwd[i]-1);

  size_t nriv = 6, nric = 4;
  size_t sriv[] = {7,     8,    10,    11,    12,    13};
  size_t sric[] = {3, 4, 5, 6};

  std::vector<size_t> riv;
  for (size_t i = 0; i < nriv; ++i)
    riv.push_back(sriv[i]-1);
  std::vector<size_t> ric;
  for (size_t i = 0; i < nric; ++i)
    ric.push_back(sric[i]-1);

  size_t nobs = 2;
  size_t svarobs[] = {2, 1};
  std::vector<size_t> varobs;
  for (size_t i = 0; i < nobs; ++i)
    varobs.push_back(svarobs[i]-1);

  Matrix Q(n_exo), H(nobs);
/*
  T(riv.size(), riv.size()), R(riv.size(), n_exo), 
  RQRt(riv.size(), riv.size()), Pstar(riv.size(), riv.size()),
  Pinf(riv.size(), riv.size()), Z(varobs.size(), riv.size()), 
  */
  
  H.setAll(0.0);
/*  for (size_t i = 0; i < varobs.size(); ++i)
    Z(i, varobs[i]) = 1.0;
*/
  MatrixView
  vCovVW(vcov, n_exo, n_exo, n_exo);
  Q = vCovVW;
  std::cout << "Matrix Q: " << std::endl << Q << std::endl;

  size_t sorder_var[] =
  {  4,  5,  6,  8,  9, 10, 11, 12, 14,  1,  7, 13,  2,  3, 15};
  std::vector<size_t> order_var;
  for (size_t ii = 0; ii < n_endo; ++ii)
    order_var.push_back(sorder_var[ii]-1);                                                                                          //= (1:endo_nbr)';

  size_t sinv_order_var[] =
  { 10, 13, 14,  1,  2,  3, 11,  4,  5,  6,  7,  8, 12,  9,  15};
  std::vector<size_t> inv_order_var;
  for (size_t ii = 0; ii < n_endo; ++ii)
    inv_order_var.push_back(sinv_order_var[ii]-1);                                                                                                          //= (1:endo_nbr)';

  double lyapunov_tol = 1e-16;
  double riccati_tol = 1e-16;
  int info = 0;
  Matrix yView(nobs,192); // dummy
  yView.setAll(0.2);
  const MatrixView dataView(yView, 0,  0, nobs, yView.getCols() ); // dummy
  Vector vll(yView.getCols());
  VectorView vwll(vll,0,vll.getSize());
 
  double penalty = 1e8;

  KalmanFilter kalman(modName, n_endo, n_exo,
                         zeta_fwrd_arg, zeta_back_arg, zeta_mixed_arg, zeta_static_arg, ll_incidence, qz_criterium,
                         order_var, inv_order_var, varobs, riv, ric, riccati_tol, lyapunov_tol, info);

  std::cout << "Initilise KF with Q: " << std::endl << Q << std::endl;
  // std::cout << "and Z" << std::endl << Z << std::endl;
  size_t start=0, period=0;
  double ll=kalman.compute(dataView, steadyState,  Q, H, deepParams,
                                   vwll, start, period, penalty, info);

  std::cout << "ll: " << std::endl << ll << std::endl;
}
