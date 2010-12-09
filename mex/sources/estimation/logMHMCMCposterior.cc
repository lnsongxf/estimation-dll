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

#include <string>
#include <vector>
#include <algorithm>
#include <functional>

#include "Vector.hh"
#include "Matrix.hh"
#include "LogPosteriorDensity.hh"
#include "RandomWalkMetropolisHastings.hh"

#include "mex.h"
#if defined MATLAB_MEX_FILE
# include "mat.h"
#else   //  OCTAVE_MEX_FILE e.t.c.
#include "matio.h"
#endif

#if defined(_WIN32) || defined(__CYGWIN32__) || defined(WINDOWS)
# define DIRECTORY_SEPARATOR "\\"
#else
# define DIRECTORY_SEPARATOR "/"
#endif

void
fillEstParamsInfo(const mxArray *estim_params_info, EstimatedParameter::pType type,
                  std::vector<EstimatedParameter> &estParamsInfo)
{
  // execute once only
  static const mxArray *bayestopt_ = mexGetVariablePtr("global", "bayestopt_");
  static const mxArray *bayestopt_ubp = mxGetField(bayestopt_, 0, "ub"); // upper bound
  static const mxArray *bayestopt_lbp = mxGetField(bayestopt_, 0, "lb"); // lower bound
  static const mxArray *bayestopt_p1p = mxGetField(bayestopt_, 0, "p1"); // prior mean
  static const mxArray *bayestopt_p2p = mxGetField(bayestopt_, 0, "p2"); // prior standard deviation
  static const mxArray *bayestopt_p3p = mxGetField(bayestopt_, 0, "p3"); // lower bound
  static const mxArray *bayestopt_p4p = mxGetField(bayestopt_, 0, "p4"); // upper bound
  static const mxArray *bayestopt_p6p = mxGetField(bayestopt_, 0, "p6"); // first hyper-parameter (\alpha for the BETA and GAMMA distributions, s for the INVERSE GAMMAs, expectation for the GAUSSIAN distribution, lower bound for the UNIFORM distribution).
  static const mxArray *bayestopt_p7p = mxGetField(bayestopt_, 0, "p7"); // second hyper-parameter (\beta for the BETA and GAMMA distributions, \nu for the INVERSE GAMMAs, standard deviation for the GAUSSIAN distribution, upper bound for the UNIFORM distribution).
  static const mxArray *bayestopt_jscalep = mxGetField(bayestopt_, 0, "jscale"); // MCMC jump scale

  static const size_t bayestopt_size = mxGetM(bayestopt_);
  static const VectorConstView bayestopt_ub(mxGetPr(bayestopt_ubp), bayestopt_size, 1);
  static const VectorConstView bayestopt_lb(mxGetPr(bayestopt_lbp), bayestopt_size, 1);
  static const VectorConstView bayestopt_p1(mxGetPr(bayestopt_p1p), bayestopt_size, 1); //=mxGetField(bayestopt_, 0, "p1");
  static const VectorConstView bayestopt_p2(mxGetPr(bayestopt_p2p), bayestopt_size, 1); //=mxGetField(bayestopt_, 0, "p2");
  static const VectorConstView bayestopt_p3(mxGetPr(bayestopt_p3p), bayestopt_size, 1); //=mxGetField(bayestopt_, 0, "p3");
  static const VectorConstView bayestopt_p4(mxGetPr(bayestopt_p4p), bayestopt_size, 1); //=mxGetField(bayestopt_, 0, "p4");
  static const VectorConstView bayestopt_p6(mxGetPr(bayestopt_p6p), bayestopt_size, 1); //=mxGetField(bayestopt_, 0, "p6");
  static const VectorConstView bayestopt_p7(mxGetPr(bayestopt_p7p), bayestopt_size, 1); //=mxGetField(bayestopt_, 0, "p7");
  static const VectorConstView bayestopt_jscale(mxGetPr(bayestopt_jscalep), bayestopt_size, 1); //=mxGetField(bayestopt_, 0, "jscale");

  // loop processsing
  size_t m = mxGetM(estim_params_info), n = mxGetN(estim_params_info);
  MatrixConstView epi(mxGetPr(estim_params_info), m, n, m);
  size_t bayestopt_count = estParamsInfo.size();

  for (size_t i = 0; i < m; i++)
    {
      size_t col = 0;
      size_t id1 = (size_t) epi(i, col++) - 1;
      size_t id2 = 0;
      if (type == EstimatedParameter::shock_Corr
          || type == EstimatedParameter::measureErr_Corr)
        id2 = (size_t) epi(i, col++) - 1;
      col++; // Skip init_val #2 or #3
      double par_low_bound =  bayestopt_lb(bayestopt_count); col++; //#3 epi(i, col++);
      double par_up_bound =  bayestopt_ub(bayestopt_count); col++; //#4 epi(i, col++);
      Prior::pShape shape = (Prior::pShape) epi(i, col++);
      double mean = epi(i, col++);
      double std = epi(i, col++);
      double low_bound =  bayestopt_p3(bayestopt_count);
      double up_bound =  bayestopt_p4(bayestopt_count);
      double fhp =  bayestopt_p6(bayestopt_count); // double p3 = epi(i, col++);
      double shp =  bayestopt_p7(bayestopt_count); // double p4 = epi(i, col++);

      Prior *p = Prior::constructPrior(shape, mean, std, low_bound, up_bound, fhp, shp); //1.0,INFINITY);//p3, p4);

      // Only one subsample
      std::vector<size_t> subSampleIDs;
      subSampleIDs.push_back(0);
      estParamsInfo.push_back(EstimatedParameter(type, id1, id2, subSampleIDs,
                                                 par_low_bound, par_up_bound, p));
      bayestopt_count++;
    }
}

int
sampleMHMC(LogPosteriorDensity &lpd, RandomWalkMetropolisHastings &rwmh,
           Matrix &steadyState, Vector &estParams, Vector &deepParams, const MatrixConstView &data, Matrix &Q, Matrix &H,
           size_t presampleStart, int &info, const VectorConstView &nruns, size_t fblock, size_t nBlocks,
           const Matrix &Jscale, Prior &drawDistribution, EstimatedParametersDescription &epd,
           const std::string &resultsFileStem, size_t console_mode, size_t load_mh_file)
{
  enum {iMin, iMax};
  int iret=0; // return value
  std::vector<size_t> OpenOldFile(nBlocks, 0);
  size_t jloop = 0, irun, j; // counters
  double dsum, dmax, dmin, sux = 0, jsux = 0;
  std::string mhFName;
  std::stringstream ssFName;
#if defined MATLAB_MEX_FILE
  MATFile *drawmat; // MCMC draws output file pointer
  int matfStatus;
#else   //  OCTAVE_MEX_FILE e.t.c.
  int dims[2];
  mat_t *drawmat;
  matvar_t *matvar;
  int matfStatus;
#endif
  FILE *fidlog;  // log file
  size_t npar = estParams.getSize();
  Matrix MinMax(npar, 2);

  const mxArray *InitSizeArrayPtr = mexGetVariablePtr("caller", "InitSizeArray");
  if (InitSizeArrayPtr==NULL)
    {
      mexPrintf("Metropolis-Hastings myinputs field InitSizeArrayPtr Initialisation failed!\n");
      return(-1);
    }
  const VectorConstView InitSizeArrayVw(mxGetPr(InitSizeArrayPtr), nBlocks, 1);
  Vector InitSizeArray(InitSizeArrayVw.getSize());
  InitSizeArray = InitSizeArrayVw;
  //const mxArray *flinePtr = mxGetField(myinputs, 0, "fline");
  const mxArray *flinePtr = mexGetVariable("caller",  "fline");
  if (flinePtr==NULL)
    {
      mexPrintf("Metropolis-Hastings myinputs field fline Initialisation failed!\n");
      return(-1);
    }
  VectorView fline(mxGetPr(flinePtr), nBlocks, 1);

  mxArray *NewFileArrayPtr = mexGetVariable("caller", "NewFile");
  if (NewFileArrayPtr==NULL)
    {
      mexPrintf("Metropolis-Hastings myinputs fields NewFileArrayPtr Initialisation failed!\n");
      return(-1);
    }
  VectorView NewFileVw(mxGetPr(NewFileArrayPtr), nBlocks, 1);
  //Vector NewFile(NewFileVw.getSize());
  //NewFile = NewFileVw;

  const mxArray *MAX_nrunsPtr = mexGetVariablePtr("caller", "MAX_nruns");
  const size_t MAX_nruns = (size_t) mxGetScalar(MAX_nrunsPtr);

  const mxArray *blockStartParamsPtr = mexGetVariable("caller", "ix2");
  MatrixView blockStartParamsMxVw(mxGetPr(blockStartParamsPtr), nBlocks, npar, nBlocks);
  Vector startParams(npar);

  const mxArray *mxFirstLogLikPtr = mexGetVariable("caller", "ilogpo2");
  VectorView FirstLogLiK(mxGetPr(mxFirstLogLikPtr), nBlocks, 1);

  const mxArray *record = mexGetVariable("caller", "record");
  //const mxArray *record = mxGetField(myinputs, 0, "record");
  if (record==NULL)
    {
      mexPrintf("Metropolis-Hastings record Initialisation failed!\n");
      return(-1);
    }
  mxArray *AcceptationRatesPtr = mxGetField(record, 0, "AcceptationRates");
  if (AcceptationRatesPtr==NULL)
    {
      mexPrintf("Metropolis-Hastings record AcceptationRatesPtr Initialisation failed!\n");
      return(-1);
    }
  VectorView AcceptationRates(mxGetPr(AcceptationRatesPtr), nBlocks, 1);

  mxArray *mxLastParametersPtr = mxGetField(record, 0, "LastParameters");
  MatrixView LastParameters(mxGetPr(mxLastParametersPtr), nBlocks, npar, nBlocks);
  LastParameters = blockStartParamsMxVw;

  mxArray *mxLastLogLikPtr = mxGetField(record, 0, "LastLogLiK");
  VectorView LastLogLiK(mxGetPr(mxLastLogLikPtr), nBlocks, 1);

  mxArray *mxMhLogPostDensPtr = 0;
  mxArray *mxMhParamDrawsPtr = 0;
  size_t currInitSizeArray = 0;

#if defined MATLAB_MEX_FILE
  // Waitbar
  mxArray *waitBarRhs[3], *waitBarLhs[1];
  waitBarRhs[0] = mxCreateDoubleMatrix(1, 1, mxREAL);
  std::string barTitle;
  std::stringstream ssbarTitle;
  if (console_mode==0)
    {
      ssbarTitle.clear();
      ssbarTitle.str("");
      ssbarTitle << "Please wait... Metropolis-Hastings  " << fblock << "/" << nBlocks << "  ...";
      barTitle = ssbarTitle.str();
      waitBarRhs[1] = mxCreateString(barTitle.c_str());
      *mxGetPr(waitBarRhs[0]) = (double) 0.0;
      mexCallMATLAB(1, waitBarLhs, 2, waitBarRhs, "waitbar");
      if (waitBarRhs[1])
        mxDestroyArray(waitBarRhs[1]);
      waitBarRhs[1] = waitBarLhs[0];
    }
#endif

  for (size_t b = fblock; b <= nBlocks; ++b)
    {
      jloop = jloop+1;

#if defined MATLAB_MEX_FILE
      if ((load_mh_file != 0)  && (fline(b) > 1) && OpenOldFile[b])
        {
          //  load(['./' MhDirectoryName '/' ModelName '_mh' int2str(NewFile(b)) '_blck' int2str(b) '.mat'])
          ssFName.clear();
          ssFName.str("");
          ssFName << resultsFileStem << DIRECTORY_SEPARATOR << "metropolis" << DIRECTORY_SEPARATOR << resultsFileStem << "_mh" << (size_t) NewFileVw(b-1) << "_blck" << b << ".mat";
          mhFName = ssFName.str();
          drawmat = matOpen(mhFName.c_str(), "r");
          mexPrintf("MHMCMC: Using interim partial draws file %s \n", mhFName.c_str());
          if (drawmat == 0)
            {
              fline(b) = 1;
              mexPrintf("Error in MH: Can not open old draws Mat file for reading:  %s \n  \
                  Starting a new file instead! \n", mhFName.c_str());
            }
          else
            {
              currInitSizeArray = (size_t) InitSizeArray(b-1);
              mxMhParamDrawsPtr = matGetVariable(drawmat, "x2");
              mxMhLogPostDensPtr = matGetVariable(drawmat, "logpo2");
              matClose(drawmat);
              OpenOldFile[b] = 1;
            }
        } // end if
      if (console_mode==0)
        {
          ssbarTitle.clear();
          ssbarTitle.str("");
          ssbarTitle << "Please wait... Metropolis-Hastings  " << b << "/" << nBlocks << "  ...";
          barTitle = ssbarTitle.str();
          waitBarRhs[2] = mxCreateString(barTitle.c_str());
          //strcpy( *mxGetPr(waitBarRhs[1]), mhFName.c_str());
          *mxGetPr(waitBarRhs[0]) = (double) 0.0;
          mexCallMATLAB(0, NULL, 3, waitBarRhs, "waitbar");
          mxDestroyArray(waitBarRhs[2]);
        }
#else //if defined OCTAVE_MEX_FILE
      if ((load_mh_file != 0)  && (fline(b) > 1) && OpenOldFile[b])
        {
          //  load(['./' MhDirectoryName '/' ModelName '_mh' int2str(NewFile(b)) '_blck' int2str(b) '.mat'])
          if ((currInitSizeArray != (size_t) InitSizeArray(b-1)) &&   OpenOldFile[b] != 1)
            {
              // new or different size result arrays/matrices
              currInitSizeArray = (size_t) InitSizeArray(b-1);
              if (mxMhLogPostDensPtr)
                mxDestroyArray(mxMhLogPostDensPtr);                                                     // log post density array
              mxMhLogPostDensPtr = mxCreateDoubleMatrix(currInitSizeArray, 1, mxREAL);
              if( mxMhLogPostDensPtr == NULL)
                {
                  mexPrintf("Metropolis-Hastings mxMhLogPostDensPtr Initialisation failed!\n");
                  return(-1);
                }
              if (mxMhParamDrawsPtr)
                mxDestroyArray(mxMhParamDrawsPtr);                                                    // accepted MCMC MH draws
              mxMhParamDrawsPtr =  mxCreateDoubleMatrix(currInitSizeArray, npar,  mxREAL);
              if( mxMhParamDrawsPtr == NULL)
                {
                  mexPrintf("Metropolis-Hastings mxMhParamDrawsPtr Initialisation failed!\n");
                  return(-1);
                }
            }

          ssFName.clear();
          ssFName.str("");
          ssFName << resultsFileStem << DIRECTORY_SEPARATOR << "metropolis" << DIRECTORY_SEPARATOR << resultsFileStem << "_mh" << (size_t) NewFileVw(b-1) << "_blck" << b << ".mat";
          mhFName = ssFName.str();
          drawmat = Mat_Open(mhFName.c_str(), MAT_ACC_RDONLY);
          if (drawmat == NULL)
            {
              fline(b) = 1;
              mexPrintf("Error in MH: Can not open old draws Mat file for reading:  %s \n  \
                  Starting a new file instead! \n", mhFName.c_str());
            }
          else
            {
              int start[2]={0,0},edge[2]={2,2},stride[2]={1,1}, err=0;
              mexPrintf("MHMCMC: Using interim partial draws file %s \n", mhFName.c_str());
//              matvar = Mat_VarReadInfo(drawmat, "x2");
              matvar = Mat_VarReadInfo(drawmat, (char *)"x2");
              if (matvar == NULL)
                {
                  fline(b) = 1;
                  mexPrintf("Error in MH: Can not read old draws Mat file for reading:  %s \n  \
                      Starting a new file instead! \n", mhFName.c_str());
                }
              else
                {
                  // GetVariable(drawmat, "x2");
                  dims[0] = matvar->dims[0]-1;
                  dims[1] = matvar->dims[1]-1;
                  err=Mat_VarReadData(drawmat,matvar,mxGetPr(mxMhParamDrawsPtr),start,stride,matvar->dims);
                  if (err)
                    {
                      fline(b) = 1;
                      mexPrintf("Error in MH: Can not retreive old draws from Mat file:  %s \n  \
                          Starting a new file instead! \n", mhFName.c_str());
                    }
                  Mat_VarFree(matvar);
                }
              //mxMhLogPostDensPtr = Mat_GetVariable(drawmat, "logpo2");
              matvar = Mat_VarReadInfo(drawmat, (char*)"logpo2");
              if (matvar == NULL)
                {
                  fline(b) = 1;
                  mexPrintf("Error in MH: Can not read old logPos Mat file for reading:  %s \n  \
                      Starting a new file instead! \n", mhFName.c_str());
                }
              else
                {
                  // GetVariable(drawmat, "x2");
                  dims[0] = matvar->dims[0]-1;
                  dims[1] = matvar->dims[1]-1;
                  err=Mat_VarReadData(drawmat,matvar,mxGetPr(mxMhLogPostDensPtr),start,stride,matvar->dims);
                  if (err)
                    {
                      fline(b) = 1;
                      mexPrintf("Error in MH: Can not retreive old logPos from Mat file:  %s \n  \
                          Starting a new file instead! \n", mhFName.c_str());
                    }
                  Mat_VarFree(matvar);
                }
              Mat_Close(drawmat);
              OpenOldFile[b] = 1;
            }
        } // end if

#endif

      VectorView LastParametersRow = mat::get_row(LastParameters, b-1);

      sux = 0.0;
      jsux = 0;
      irun = (size_t) fline(b-1);
      j = 0; //1;
      while (j < nruns(b-1))
        {
          if ((currInitSizeArray != (size_t) InitSizeArray(b-1)) &&   OpenOldFile[b] != 1)
            {
              // new or different size result arrays/matrices
              currInitSizeArray = (size_t) InitSizeArray(b-1);
              if (mxMhLogPostDensPtr)
                mxDestroyArray(mxMhLogPostDensPtr);                                                     // log post density array
              mxMhLogPostDensPtr = mxCreateDoubleMatrix(currInitSizeArray, 1, mxREAL);
              if( mxMhLogPostDensPtr == NULL)
                {
                  mexPrintf("Metropolis-Hastings mxMhLogPostDensPtr Initialisation failed!\n");
                  return(-1);
                }
              if (mxMhParamDrawsPtr)
                mxDestroyArray(mxMhParamDrawsPtr);                                                    // accepted MCMC MH draws
              mxMhParamDrawsPtr =  mxCreateDoubleMatrix(currInitSizeArray, npar,  mxREAL);
              if( mxMhParamDrawsPtr == NULL)
                {
                  mexPrintf("Metropolis-Hastings mxMhParamDrawsPtr Initialisation failed!\n");
                  return(-1);
                }
            }
          startParams = LastParametersRow;
          VectorView mhLogPostDens(mxGetPr(mxMhLogPostDensPtr), currInitSizeArray, (size_t) 1);
          MatrixView mhParamDraws(mxGetPr(mxMhParamDrawsPtr), currInitSizeArray, npar, currInitSizeArray);
          try
            {
              jsux = rwmh.compute(mhLogPostDens, mhParamDraws, steadyState, startParams, deepParams, data, Q, H,
                              presampleStart, info, irun, currInitSizeArray, Jscale, lpd, drawDistribution, epd);
              irun = currInitSizeArray;
              sux += jsux*currInitSizeArray;
              j += currInitSizeArray; //j=j+1;
            }
          catch (const TSException &tse)
            {
              iret=-100;
              mexPrintf(" TSException Exception in RandomWalkMH dynamic_dll: %s \n", (tse.getMessage()).c_str());
              goto cleanup;
            }
          catch (const DecisionRules::BlanchardKahnException &bke)
            {
              iret=-90;
              mexPrintf(" Too many Blanchard-Kahn Exceptions in RandomWalkMH : n_fwrd_vars %d n_explosive_eigenvals %d \n", bke.n_fwrd_vars, bke.n_explosive_eigenvals);
              goto cleanup;
            } 
          catch (const GeneralizedSchurDecomposition::GSDException &gsde)
            {
              iret=-80;
              mexPrintf(" GeneralizedSchurDecomposition Exception in RandomWalkMH: info %d, n %d  \n", gsde.info, gsde.n);
              goto cleanup;
            }
          catch (const LUSolver::LUException &lue)
            {
              iret=-70;
              mexPrintf(" LU Exception in RandomWalkMH : info %d \n", lue.info);
              goto cleanup;
            }
          catch (const VDVEigDecomposition::VDVEigException &vdve)
            {
              iret=-60;
              mexPrintf(" VDV Eig Exception in RandomWalkMH : %s ,  info: %d\n", vdve.message.c_str(), vdve.info);
              goto cleanup;
            }
          catch (const DiscLyapFast::DLPException &dlpe)
            {
              iret=-50;
              mexPrintf(" Lyapunov solver Exception in RandomWalkMH : %s ,  info: %d\n", dlpe.message.c_str(), dlpe.info);
              goto cleanup;
            }
          catch (const std::runtime_error &re)
            {
              iret=-3;
              mexPrintf(" Runtime Error Exception in RandomWalkMH: %s \n", re.what());
              goto cleanup;
            }
          catch (const std::exception &e)
            {
              iret=-2;
              mexPrintf(" Standard System Exception in RandomWalkMH: %s \n", e.what());
              goto cleanup;
            }
          catch (...)
            {
              iret=-1000;
              mexPrintf(" Unknown unhandled Exception in RandomWalkMH! %s \n");
              goto cleanup;
            }

#if defined MATLAB_MEX_FILE
          if (console_mode)
            mexPrintf("   MH: Computing Metropolis-Hastings (chain %d/%d): %3.f \b%% done, acceptance rate: %3.f \b%%\r", b, nBlocks, 100 * j/nruns(b-1), 100 * sux / j);
          else
            {
              // Waitbar
              ssbarTitle.clear();
              ssbarTitle.str("");
              ssbarTitle << "Metropolis-Hastings : " << b << "/" << nBlocks << " Acceptance: " << 100 * sux/j << "%";
              barTitle = ssbarTitle.str();
              waitBarRhs[2] = mxCreateString(barTitle.c_str());
              *mxGetPr(waitBarRhs[0]) = j / nruns(b-1);
              mexCallMATLAB(0, NULL, 3, waitBarRhs, "waitbar");
              mxDestroyArray(waitBarRhs[2]);

            }

          // % Now I save the simulations
          // save draw  2 mat file ([MhDirectoryName '/' ModelName '_mh' int2str(NewFile(b)) '_blck' int2str(b) '.mat'],'x2','logpo2');
          ssFName.clear();
          ssFName.str("");
          ssFName << resultsFileStem << DIRECTORY_SEPARATOR << "metropolis" << DIRECTORY_SEPARATOR << resultsFileStem << "_mh" << (size_t) NewFileVw(b-1) << "_blck" << b << ".mat";
          mhFName = ssFName.str();
          drawmat = matOpen(mhFName.c_str(), "w");
          if (drawmat == 0)
            {
              mexPrintf("Error in MH: Can not open draws Mat file for writing:  %s \n", mhFName.c_str());
              exit(1);
            }
          matfStatus = matPutVariable(drawmat, "x2", mxMhParamDrawsPtr);
          if (matfStatus)
            {
              mexPrintf("Error in MH: Can not use draws Mat file for writing:  %s \n", mhFName.c_str());
              exit(1);
            }
          matfStatus = matPutVariable(drawmat, "logpo2", mxMhLogPostDensPtr);
          if (matfStatus)
            {
              mexPrintf("Error in MH: Can not usee draws Mat file for writing:  %s \n", mhFName.c_str());
              exit(1);
            }
          matClose(drawmat);
#else

          printf("   MH: Computing Metropolis-Hastings (chain %d/%d): %3.f \b%% done, acceptance rate: %3.f \b%%\r", b, nBlocks, 100 * j/nruns(b-1), 100 * sux / j);
          // % Now I save the simulations
          // save draw  2 mat file ([MhDirectoryName '/' ModelName '_mh' int2str(NewFile(b)) '_blck' int2str(b) '.mat'],'x2','logpo2');
          ssFName.clear();
          ssFName.str("");
          ssFName << resultsFileStem << DIRECTORY_SEPARATOR << "metropolis" << DIRECTORY_SEPARATOR << resultsFileStem << "_mh" << (size_t) NewFileVw(b-1) << "_blck" << b << ".mat";
          mhFName = ssFName.str();

          drawmat = Mat_Open(mhFName.c_str(), MAT_ACC_RDWR);
          if (drawmat == 0)
            {
              mexPrintf("Error in MH: Can not open draws Mat file for writing:  %s \n", mhFName.c_str());
              exit(1);
            }
          dims[0]= currInitSizeArray;
          dims[1]= npar;
          matvar = Mat_VarCreate("x2",MAT_C_DOUBLE,MAT_T_DOUBLE,2,dims,mxGetPr(mxMhParamDrawsPtr),0);
          matfStatus=Mat_VarWrite( drawmat, matvar, 0);
          Mat_VarFree(matvar);          
          if (matfStatus)
            {
              mexPrintf("Error in MH: Can not use draws Mat file for writing:  %s \n", mhFName.c_str());
              exit(1);
            }
          //matfStatus = matPutVariable(drawmat, "logpo2", mxMhLogPostDensPtr);
          dims[1]= 1;
          matvar = Mat_VarCreate("logpo2",MAT_C_DOUBLE,MAT_T_DOUBLE,2,dims,mxGetPr(mxMhLogPostDensPtr),0);
          matfStatus=Mat_VarWrite( drawmat, matvar, 0);
          Mat_VarFree(matvar);          
          if (matfStatus)
            {
              mexPrintf("Error in MH: Can not usee draws Mat file for writing:  %s \n", mhFName.c_str());
              exit(1);
            }
          Mat_Close(drawmat);
#endif

          // save log to fidlog = fopen([MhDirectoryName '/metropolis.log'],'a');
          ssFName.str("");
          ssFName << resultsFileStem << DIRECTORY_SEPARATOR << "metropolis" << DIRECTORY_SEPARATOR << "metropolis.log";
          mhFName = ssFName.str();
          fidlog = fopen(mhFName.c_str(), "a");
          fprintf(fidlog, "\n");
          fprintf(fidlog, "%% Mh%dBlck%d ( %s %s )\n", (int) NewFileVw(b-1), b, __DATE__, __TIME__);
          fprintf(fidlog, " \n");
          fprintf(fidlog, "  Number of simulations.: %d \n", currInitSizeArray); // (length(logpo2)) ');
          fprintf(fidlog, "  Acceptation rate......: %f \n", jsux);
          fprintf(fidlog, "  Posterior mean........:\n");
          for (size_t i = 0; i < npar; ++i)
            {
              VectorView mhpdColVw = mat::get_col(mhParamDraws, i);
              fprintf(fidlog, "    params: %d : %f \n", i+1, vec::meanSumMinMax(dsum, dmin, dmax, mhpdColVw));
              MinMax(i, iMin) = dmin;
              MinMax(i, iMax) = dmax;
            } // end
          fprintf(fidlog, "    log2po: %f \n", vec::meanSumMinMax(dsum, dmin, dmax, mhLogPostDens));
          fprintf(fidlog, "  Minimum value.........:\n");;
          for (size_t i = 0; i < npar; ++i)
            fprintf(fidlog, "    params: %d : %f \n", i+1, MinMax(i, iMin));
          fprintf(fidlog, "    log2po: %f \n", dmin);
          fprintf(fidlog, "  Maximum value.........:\n");
          for (size_t i = 0; i < npar; ++i)
            fprintf(fidlog, "    params: %d : %f \n", i+1, MinMax(i, iMax));
          fprintf(fidlog, "    log2po: %f \n", dmax);
          fprintf(fidlog, " \n");
          fclose(fidlog);

          jsux = 0;
          LastParametersRow = mat::get_row(mhParamDraws, currInitSizeArray-1); //x2(end,:);
          LastLogLiK(b-1) = mhLogPostDens(currInitSizeArray-1); //logpo2(end);
          InitSizeArray(b-1) = std::min( (size_t) nruns(b-1)-j, MAX_nruns);
          // initialization of next file if necessary
          if (InitSizeArray(b-1))
            {
              NewFileVw(b-1)++;// = NewFile(b-1) + 1;
              irun = 1;
            } // end
          //irun++;
        } // end  while % End of the simulations for one mh-block.
      //record.
      AcceptationRates(b-1) = sux/j;
      OpenOldFile[b] = 0;
    } // end % End of the loop over the mh-blocks.

  if (mexPutVariable("caller", "record_AcceptationRates", AcceptationRatesPtr))
    mexPrintf("MH Warning: due to error record_AcceptationRates is NOT set !! \n");

  if (mexPutVariable("caller", "record_LastParameters", mxLastParametersPtr))
    mexPrintf("MH Warning: due to error record_MhParamDraw is NOT set !! \n");

  if (mexPutVariable("caller", "record_LastLogLiK", mxLastLogLikPtr))
    mexPrintf("MH Warning: due to error record_LastLogLiK is NOT set !! \n");

  //NewFileVw = NewFile;
  if (mexPutVariable("caller", "NewFile", NewFileArrayPtr))
    mexPrintf("MH Warning: due to error NewFile is NOT set !! \n");

  // Cleanup
mexPrintf("MH Cleanup !! \n");

cleanup:
  if (mxMhLogPostDensPtr)
    mxDestroyArray(mxMhLogPostDensPtr);   // delete log post density array
  if (mxMhParamDrawsPtr)
    mxDestroyArray(mxMhParamDrawsPtr);    // delete accepted MCMC MH draws

#ifdef MATLAB_MEX_FILE
  // Waitbar  
  if (console_mode==0)
    {
      // Bellow call to close waitbar seems to cause crashes and it is for 
      // now left commented out and the waitbar neeeds to be closed manually
      // alternativelly, call with options_.console_mode=1;  
      //mexCallMATLAB(0, NULL, 1, waitBarLhs, "close");
      //mxDestroyArray(waitBarLhs[0]);
      mxDestroyArray(waitBarRhs[1]);
      mxDestroyArray(waitBarRhs[0]);
    }
#endif

  // return error code or last line run in the last MH block sub-array
  if (iret==0)
    iret=(int)irun;
  return iret;

}

int
logMCMCposterior(const VectorConstView &estParams, const MatrixConstView &data, const std::string &mexext,
                 const size_t fblock, const size_t nBlocks, const VectorConstView &nMHruns, const MatrixConstView &D)
{
  // Retrieve pointers to global variables
  const mxArray *M_ = mexGetVariablePtr("global", "M_");
  const mxArray *oo_ = mexGetVariablePtr("global", "oo_");
  const mxArray *options_ = mexGetVariablePtr("global", "options_");
  const mxArray *estim_params_ = mexGetVariablePtr("global", "estim_params_");

  // Construct arguments of constructor of LogLikelihoodMain
  char *fName = mxArrayToString(mxGetField(M_, 0, "fname"));
  std::string resultsFileStem(fName);
  std::string dynamicDllFile(fName);
  mxFree(fName);
  dynamicDllFile += "_dynamic." + mexext;

  size_t n_endo = (size_t) *mxGetPr(mxGetField(M_, 0, "endo_nbr"));
  size_t n_exo = (size_t) *mxGetPr(mxGetField(M_, 0, "exo_nbr"));
  size_t n_param = (size_t) *mxGetPr(mxGetField(M_, 0, "param_nbr"));
  size_t n_estParams = estParams.getSize();

  std::vector<size_t> zeta_fwrd, zeta_back, zeta_mixed, zeta_static;
  const mxArray *lli_mx = mxGetField(M_, 0, "lead_lag_incidence");
  MatrixConstView lli(mxGetPr(lli_mx), mxGetM(lli_mx), mxGetN(lli_mx), mxGetM(lli_mx));
  if (lli.getRows() != 3 || lli.getCols() != n_endo)
    mexErrMsgTxt("Incorrect lead/lag incidence matrix");
  for (size_t i = 0; i < n_endo; i++)
    {
      if (lli(0, i) == 0 && lli(2, i) == 0)
        zeta_static.push_back(i);
      else if (lli(0, i) != 0 && lli(2, i) == 0)
        zeta_back.push_back(i);
      else if (lli(0, i) == 0 && lli(2, i) != 0)
        zeta_fwrd.push_back(i);
      else
        zeta_mixed.push_back(i);
    }

  double qz_criterium = *mxGetPr(mxGetField(options_, 0, "qz_criterium"));
  double lyapunov_tol = *mxGetPr(mxGetField(options_, 0, "lyapunov_complex_threshold"));
  double riccati_tol = *mxGetPr(mxGetField(options_, 0, "riccati_tol"));
  size_t presample = (size_t) *mxGetPr(mxGetField(options_, 0, "presample"));
  size_t console_mode = (size_t) *mxGetPr(mxGetField(options_, 0, "console_mode"));
  size_t load_mh_file = (size_t) *mxGetPr(mxGetField(options_, 0, "load_mh_file"));

  std::vector<size_t> varobs;
  const mxArray *varobs_mx = mxGetField(options_, 0, "varobs_id");
  if (mxGetM(varobs_mx) != 1)
    mexErrMsgTxt("options_.varobs_id must be a row vector");
  size_t n_varobs = mxGetN(varobs_mx);
  std::transform(mxGetPr(varobs_mx), mxGetPr(varobs_mx) + n_varobs, back_inserter(varobs),
                 std::bind2nd(std::minus<size_t>(), 1));

  if (data.getRows() != n_varobs)
    mexErrMsgTxt("Data has not as many rows as there are observed variables");

  std::vector<EstimationSubsample> estSubsamples;
  estSubsamples.push_back(EstimationSubsample(0, data.getCols() - 1));

  std::vector<EstimatedParameter> estParamsInfo;
  fillEstParamsInfo(mxGetField(estim_params_, 0, "var_exo"), EstimatedParameter::shock_SD,
                    estParamsInfo);
  fillEstParamsInfo(mxGetField(estim_params_, 0, "var_endo"), EstimatedParameter::measureErr_SD,
                    estParamsInfo);
  fillEstParamsInfo(mxGetField(estim_params_, 0, "corrx"), EstimatedParameter::shock_Corr,
                    estParamsInfo);
  fillEstParamsInfo(mxGetField(estim_params_, 0, "corrn"), EstimatedParameter::measureErr_Corr,
                    estParamsInfo);
  fillEstParamsInfo(mxGetField(estim_params_, 0, "param_vals"), EstimatedParameter::deepPar,
                    estParamsInfo);
  EstimatedParametersDescription epd(estSubsamples, estParamsInfo);

  // Allocate LogPosteriorDensity object
  int info;
  LogPosteriorDensity lpd(dynamicDllFile, epd, n_endo, n_exo, zeta_fwrd, zeta_back, zeta_mixed, zeta_static,
                          qz_criterium, varobs, riccati_tol, lyapunov_tol, info);

  // Construct arguments of compute() method
  Matrix steadyState(n_endo, 1);
  mat::get_col(steadyState, 0) = VectorConstView(mxGetPr(mxGetField(oo_, 0, "steady_state")), n_endo, 1);

  Vector estParams2(n_estParams);
  estParams2 = estParams;
  Vector deepParams(n_param);
  deepParams = VectorConstView(mxGetPr(mxGetField(M_, 0, "params")), n_param, 1);
  Matrix Q(n_exo);
  Q = MatrixConstView(mxGetPr(mxGetField(M_, 0, "Sigma_e")), n_exo, n_exo, n_exo);

  Matrix H(n_varobs);
  const mxArray *H_mx = mxGetField(M_, 0, "H");
  if (mxGetM(H_mx) == 1 && mxGetN(H_mx) == 1 && *mxGetPr(H_mx) == 0)
    H.setAll(0.0);
  else
    H = MatrixConstView(mxGetPr(mxGetField(M_, 0, "H")), n_varobs, n_varobs, n_varobs);

  // Construct MHMCMC Sampler
  RandomWalkMetropolisHastings rwmh(estParams2.getSize());
  // Construct GaussianPrior drawDistribution m=0, sd=1
  GaussianPrior drawGaussDist01(0.0, 1.0, -INFINITY, INFINITY, 0.0, 1.0);
  // get Jscale = diag(bayestopt_.jscale);
  const mxArray *bayestopt_ = mexGetVariablePtr("global", "bayestopt_");
  Matrix Jscale(n_estParams);
  Matrix Dscale(n_estParams);
  //Vector vJscale(n_estParams);
  Jscale.setAll(0.0);
  VectorConstView vJscale(mxGetPr(mxGetField(bayestopt_, 0, "jscale")), n_estParams, 1);
  for (size_t i = 0; i < n_estParams; i++)
    Jscale(i, i) = vJscale(i);
  blas::gemm("N", "N", 1.0, D, Jscale, 0.0, Dscale);

  //sample MHMCMC draws and get get last line run in the last MH block sub-array
  int lastMHblockArrayLine = sampleMHMC(lpd, rwmh, steadyState, estParams2, deepParams, data, Q, H, presample, info,
                                           nMHruns, fblock, nBlocks, Dscale, drawGaussDist01, epd, resultsFileStem, console_mode, load_mh_file);

  // Cleanups
  for (std::vector<EstimatedParameter>::iterator it = estParamsInfo.begin();
       it != estParamsInfo.end(); it++)
    delete it->prior;

  return lastMHblockArrayLine;
}

void
mexFunction(int nlhs, mxArray *plhs[],
            int nrhs, const mxArray *prhs[])
{
  if (nrhs != 7)
    mexErrMsgTxt("logposterior: exactly seven arguments are required.");
  if (nlhs != 1)
    mexErrMsgTxt("logposterior: exactly one return argument is required.");

  plhs[0] = mxCreateDoubleMatrix(1, 1, mxREAL);
// Check and retrieve the arguments

  if (!mxIsDouble(prhs[0]) || mxGetN(prhs[0]) != 1)
    mexErrMsgTxt("logposterior: First argument must be a column vector of double-precision numbers");

  VectorConstView estParams(mxGetPr(prhs[0]), mxGetM(prhs[0]), 1);

  if (!mxIsDouble(prhs[1]))
    mexErrMsgTxt("logposterior: Second argument must be a matrix of double-precision numbers");

  MatrixConstView data(mxGetPr(prhs[1]), mxGetM(prhs[1]), mxGetN(prhs[1]), mxGetM(prhs[1]));

  if (!mxIsChar(prhs[2]))
    mexErrMsgTxt("logposterior: Third argument must be a character string");

  char *mexext_mx = mxArrayToString(prhs[2]);
  std::string mexext(mexext_mx);
  mxFree(mexext_mx);

  size_t fblock = (size_t) mxGetScalar(prhs[3]);
  size_t nBlocks = (size_t) mxGetScalar(prhs[4]);
  VectorConstView nMHruns(mxGetPr(prhs[5]), mxGetM(prhs[5]), 1);
  assert(nMHruns.getSize() == nBlocks);

  MatrixConstView D(mxGetPr(prhs[6]), mxGetM(prhs[6]), mxGetN(prhs[6]), mxGetM(prhs[6]));
  //calculate MHMCMC draws and get get last line run in the last MH block sub-array
  int lastMHblockArrayLine = logMCMCposterior(estParams, data, mexext, fblock, nBlocks, nMHruns, D);

  *mxGetPr(plhs[0]) = (double) lastMHblockArrayLine;
}
