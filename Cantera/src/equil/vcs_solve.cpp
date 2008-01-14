/*!
 * @file vcs_solve.h
 *    Header file for the internal class that holds the problem.
 */
/*
 * $Id$
 */
/*
 * Copywrite (2005) Sandia Corporation. Under the terms of 
 * Contract DE-AC04-94AL85000 with Sandia Corporation, the
 * U.S. Government retains certain rights in this software.
 */


#include "vcs_solve.h"
#include "vcs_Exception.h"
#include "vcs_internal.h"
#include "vcs_prob.h"

#include "vcs_VolPhase.h"
#include "vcs_SpeciesProperties.h"
#include "vcs_species_thermo.h"

#include <string>
#include "math.h"
using namespace std;

namespace VCSnonideal {

  VCS_SOLVE::VCS_SOLVE() :
    NSPECIES0(0),
    NPHASE0(0),
    m_numSpeciesTot(0),
    m_numElemConstraints(0),
    m_numComponents(0),
    m_numRxnTot(0),
    m_numSpeciesRdc(0),
    m_numRxnMinorZeroed(0),
    NPhase(0),
    iest(0),
    TMoles(0.0),
    T(0.0),
    Pres(0.0),
    tolmaj(0.0),
    tolmin(0.0),
    tolmaj2(0.0),
    tolmin2(0.0),
    UnitsState(VCS_DIMENSIONAL_G),
    UseActCoeffJac(0),
    Vol(0.0),
    Faraday_dim(1.602e-19 * 6.022136736e26),
    m_VCount(0),
#ifdef DEBUG_MODE
    vcs_debug_print_lvl(0),
#endif
    m_VCS_UnitsFormat(VCS_UNITS_UNITLESS)
  {
  }

  void VCS_SOLVE::InitSizes(int nspecies0, int nelements, int nphase0) 
  {
    if (NSPECIES0 != 0) {
      if ((nspecies0 != NSPECIES0) || (nelements != m_numElemConstraints) || (nphase0 != NPHASE0)){
	delete_memory();
      } else {
	return;
      }
    }

    NSPECIES0 = nspecies0;
    NPHASE0 = nphase0;
    m_numSpeciesTot = nspecies0;
    m_numElemConstraints = nelements;
    m_numComponents = nelements;

    int iph;
    string ser = "VCS_SOLVE: ERROR:\n\t";
    if (nspecies0 <= 0) {
      plogf("%s Number of species is nonpositive\n", ser.c_str());
      throw vcsError("VCS_SOLVE()", 
		     ser + " Number of species is nonpositive\n",
		     VCS_PUB_BAD);
    }
    if (nelements <= 0) {
      plogf("%s Number of elements is nonpositive\n", ser.c_str());
      throw vcsError("VCS_SOLVE()", 
		     ser + " Number of species is nonpositive\n",
		     VCS_PUB_BAD);
    }
    if (nphase0 <= 0) {
      plogf("%s Number of phases is nonpositive\n", ser.c_str());
      throw vcsError("VCS_SOLVE()", 
		     ser + " Number of species is nonpositive\n",
		     VCS_PUB_BAD);
    }

    //vcs_priv_init(this);
    m_VCS_UnitsFormat = VCS_UNITS_UNITLESS;
   
    /*
     * We will initialize sc[] to note the fact that it needs to be
     * filled with meaningful information.
     */
    sc.resize(nspecies0, nelements, 0.0);

    scSize.resize(nspecies0, 0.0);

    m_gibbsSpecies.resize(nspecies0, 0.0);
    ff.resize(nspecies0, 0.0);
    feTrial.resize(nspecies0, 0.0);
    soln.resize(nspecies0, 0.0);

    SpeciesUnknownType.resize(nspecies0, VCS_SPECIES_TYPE_MOLNUM);

    DnPhase.resize(nspecies0, nphase0, 0.0);
    PhaseParticipation.resize(nspecies0, nphase0, 0);
    phasePhi.resize(nphase0, 0.0);

    wt.resize(nspecies0, 0.0);

    dg.resize(nspecies0, 0.0);
    dgl.resize(nspecies0, 0.0);
    ds.resize(nspecies0, 0.0);

    fel.resize(nspecies0, 0.0);
    ga.resize(nelements, 0.0);
    gai.resize(nelements, 0.0);
   

    TPhMoles.resize(nphase0, 0.0);
    TPhMoles1.resize(nphase0, 0.0);
    DelTPhMoles.resize(nphase0, 0.0);
    TmpPhase.resize(nphase0, 0.0);
    TmpPhase2.resize(nphase0, 0.0);
  
    FormulaMatrix.resize(nelements, nspecies0);

    TPhInertMoles.resize(nphase0, 0.0);

    /*
     * ind[] is an index variable that keep track of solution vector
     * rotations.
     */
    ind.resize(nspecies0, 0);
    indPhSp.resize(nspecies0, 0);
    /*
     *    IndEl[] is an index variable that keep track of element vector
     *    rotations.
     */
    IndEl.resize(nelements, 0);

    /*
     *   ir[] is an index vector that keeps track of the irxn to species
     *   mapping. We can't fill it in until we know the number of c
     *   components in the problem
     */
    ir.resize(nspecies0, 0);
   
    /* Initialize all species to be major species */
    spStatus.resize(nspecies0, 1);
   
    SSPhase.resize(2*nspecies0, 0);
    PhaseID.resize(nspecies0, 0);

    m_numElemConstraints  = nelements;

    ElName.resize(nelements, std::string(""));
    SpName.resize(nspecies0, std::string(""));

    m_elType.resize(nelements, VCS_ELEM_TYPE_ABSPOS);

    ElActive.resize(nelements,  1);
    /*
     *    Malloc space for activity coefficients for all species
     *    -> Set it equal to one.
     */
    SpecActConvention.resize(nspecies0, 0);
    PhaseActConvention.resize(nphase0, 0);
    SpecLnMnaught.resize(nspecies0, 0.0);
    ActCoeff.resize(nspecies0, 1.0);
    ActCoeff0.resize(nspecies0, 1.0);
    CurrPhAC.resize(nphase0, 0);
    WtSpecies.resize(nspecies0, 0.0);
    Charge.resize(nspecies0, 0.0);
    SpeciesThermo.resize(nspecies0, (VCS_SPECIES_THERMO *)0);
   
    /*
     *    Malloc Phase Info
     */
    VPhaseList.resize(nphase0, 0);
    for (iph = 0; iph < nphase0; iph++) {
      VPhaseList[iph] = new vcs_VolPhase();
    }   
   
    /*
     *        For Future expansion
     */
    UseActCoeffJac = true;
    if (UseActCoeffJac ) {
      dLnActCoeffdMolNum.resize(nspecies0, nspecies0, 0.0);
    }
   
    VolPM.resize(nspecies0, 0.0);
   
    /*
     *        Malloc space for counters kept within vcs
     *
     */
    m_VCount    = new VCS_COUNTERS();
    vcs_counters_init(1);

  
    return;

  }


  /**
   *
   */
  VCS_SOLVE::~VCS_SOLVE()
  {
    delete_memory();
  }

  void VCS_SOLVE::delete_memory(void) 
  {
    int j, nph = NPhase;
    int nspecies = m_numSpeciesTot;
   
    for (j = 0; j < nph; j++) {
      delete VPhaseList[j];
      VPhaseList[j] = 0;
    }

    for (j = 0; j < nspecies; j++) {
      delete SpeciesThermo[j];
      SpeciesThermo[j] = 0;
    } 
   
    delete m_VCount; m_VCount = 0;

    NSPECIES0 = 0;
    NPHASE0 = 0;
    m_numElemConstraints = 0;
    m_numComponents = 0;
  }

  /*****************************************************************************/
  /*****************************************************************************/
  /*****************************************************************************/
  // Solve an equilibrium problem
  /*
   *  This is the main interface routine to the equilibrium solver
   *
   * Input:
   *   @param vprob Object containing the equilibrium Problem statement
   *   
   *   @param ifunc Determines the operation to be done: Valid values:
   *            0 -> Solve a new problem by initializing structures
   *                 first. An initial estimate may or may not have
   *                 been already determined. This is indicated in the
   *                 VCS_PROB structure.
   *            1 -> The problem has already been initialized and 
   *                 set up. We call this routine to resolve it 
   *                 using the problem statement and
   *                 solution estimate contained in 
   *                 the VCS_PROB structure.
   *            2 -> Don't solve a problem. Destroy all the private 
   *                 structures.
   *
   *  @param ipr Printing of results
   *     ipr = 1 -> Print problem statement and final results to 
   *                standard output 
   *           0 -> don't report on anything 
   *  @param ip1 Printing of intermediate results
   *     ip1 = 1 -> Print intermediate results. 
   *         = 0 -> No intermediate results printing
   *
   *  @param maxit  Maximum number of iterations for the algorithm 
   *
   *  @param iprintTime Printing of time information. Default = -1,
   *                    implies printing if other printing is turned on.
   *
   * Output:
   *
   *    @return
   *       nonzero value: failure to solve the problem at hand.
   *       zero : success
   */
  int VCS_SOLVE::vcs(VCS_PROB *vprob, int ifunc, int ipr, int ip1, int maxit,
		     int iprintTime) {
    int retn = 0;
    int iconv = 0, nspecies0, nelements0, nphase0;
    double te, ts = vcs_second();
    if (iprintTime == -1) {
      iprintTime = MAX(ipr, ip1);
    }
    if (ifunc < 0 || ifunc > 2) {
      plogf("vcs: Unrecognized value of ifunc, %d: bailing!\n",
	     ifunc);
      return VCS_PUB_BAD;
    }
  
    if (ifunc == 0) {
      /*
       *         This function is called to create the private data
       *         using the public data.
       */
      nspecies0 = vprob->nspecies + 10;
      nelements0 = vprob->ne;
      nphase0 = vprob->NPhase;
    
      InitSizes(nspecies0, nelements0, nphase0);
     
      if (retn != 0) {
	plogf("vcs_priv_alloc returned a bad status, %d: bailing!\n",
	       retn);
	return retn;
      }
      /*
       *         This function is called to copy the public data
       *         and the current problem specification
       *         into the current object's data structure.
       */
      retn = vcs_prob_specifyFully(vprob);
      if (retn != 0) {
	plogf("vcs_pub_to_priv returned a bad status, %d: bailing!\n",
	       retn);
	return retn;
      }
      /*
       *        Prep the problem data
       *           - adjust the identity of any phases
       *           - determine the number of components in the problem
       */
      retn = vcs_prep_oneTime(ip1);
      if (retn != 0) {
	plogf("vcs_prep_oneTime returned a bad status, %d: bailing!\n",
	       retn);
	return retn;
      }      
    }
    if (ifunc == 1) {
      /*
       *         This function is called to copy the current problem
       *         into the current object's data structure.
       */
      retn = vcs_prob_specify(vprob);
      if (retn != 0) {
	plogf("vcs_prob_specify returned a bad status, %d: bailing!\n",
	       retn);
	return retn;
      }
    }
    if (ifunc != 2) {
      /*
       *        Prep the problem data for this particular instantiation of
       *        the problem
       */
      retn = vcs_prep();
      if (retn != VCS_SUCCESS) {
	plogf("vcs_prep returned a bad status, %d: bailing!\n", retn);
	return retn;
      }

      /*
       *        Check to see if the current problem is well posed.
       */
      if (!vcs_wellPosed(vprob)) {
	plogf("vcs has determined the problem is not well posed: Bailing\n");
	return VCS_PUB_BAD;
      }
       
      /*
       *      Once we have defined the global internal data structure defining
       *      the problem, then we go ahead and solve the problem.
       *
       *   (right now, all we do is solve fixed T, P problems.
       *    Methods for other problem types will go in at this level.
       *    For example, solving for fixed T, V problems will involve
       *    a 2x2 Newton's method, using loops over vcs_TP() to
       *    calculate the residual and Jacobian)
       */
      switch (vprob->prob_type) {
      case VCS_PROBTYPE_TP:
	iconv = vcs_TP(ipr, ip1, maxit, vprob->T, vprob->Pres);
	break;
      case VCS_PROBTYPE_TV:
	iconv = vcs_TV(ipr, ip1, maxit, vprob->T, vprob->Vol);	    
	break;
      default:
	plogf("Unknown or unimplemented problem type: %d\n",
	       vprob->prob_type);
	return VCS_PUB_BAD;
      }
	     
      /*
       *        If requested to print anything out, go ahead and do so;
       */
      if (ipr) vcs_report(iconv);
      /*
       *        Copy the results of the run back to the VCS_PROB structure,
       *        which is returned to the user.
       */
      vcs_prob_update(vprob);
    }

    /*
     * Report on the time if requested to do so
     */
    te = vcs_second();
    m_VCount->T_Time_vcs += te - ts;
    if (iprintTime > 0) {
      vcs_TCounters_report();
    }
    /*
     *        Now, destroy the private data, if requested to do so
     *
     * FILL IN
     */
   
    if (iconv < 0) {
      plogf("ERROR: FAILURE its = %d!\n", m_VCount->Its);
    } else if (iconv == 1) {
      plogf("WARNING: RANGE SPACE ERROR encountered\n");
    }
    return iconv;
  }
  /*****************************************************************************/
  /*****************************************************************************/
  /*****************************************************************************/
  // Fully specify the problem to be solved using VCS_PROB
  /*
   *  Use the contents of the VCS_PROB to specify the contents of the
   *  private data, VCS_SOLVE.
   *
   *  @param pub  Pointer to VCS_PROB that will be used to
   *              initialize the current equilibrium problem
   */
  int VCS_SOLVE::vcs_prob_specifyFully(const VCS_PROB *pub) {
    int i, j,  kspec;
    int iph;
    vcs_VolPhase *Vphase = 0;
    const char *ser =
      "vcs_pub_to_priv ERROR :ill defined interface -> bailout:\n\t";

    /*
     *  First Check to see whether we have room for the current problem
     *  size
     */
    int nspecies  = pub->nspecies;
    if (NSPECIES0 < nspecies) {
      plogf("%sPrivate Data is dimensioned too small\n", ser);
      return VCS_PUB_BAD;
    }
    int nph = pub->NPhase;
    if (NPHASE0 < nph) {
      plogf("%sPrivate Data is dimensioned too small\n", ser);
      return VCS_PUB_BAD;
    }
    int nelements = pub->ne;
    if (m_numElemConstraints < nelements) {
      plogf("%sPrivate Data is dimensioned too small\n", ser);
      return VCS_PUB_BAD;
    }

    /*
     * OK, We have room. Now, transfer the integer numbers
     */
    m_numElemConstraints     = nelements;
    m_numSpeciesTot = nspecies;
    m_numSpeciesRdc = m_numSpeciesTot;
    /*
     *  nc = number of components -> will be determined later. 
     *       but set it to its maximum possible value here.
     */
    m_numComponents     = nelements;
    /*
     *   m_numRxnTot = number of noncomponents, also equal to the
     *                 number of reactions
     */
    m_numRxnTot = MAX(nspecies - nelements, 0);
    m_numRxnRdc = m_numRxnTot;
    /*
     *  number of minor species rxn -> all species rxn are major at the start.
     */
    m_numRxnMinorZeroed = 0;
    /*
     *  NPhase = number of phases
     */
    NPhase = nph;

#ifdef DEBUG_MODE
    vcs_debug_print_lvl = pub->vcs_debug_print_lvl;
#endif

    /*
     * FormulaMatrix[] -> Copy the formula matrix over
     */
    for (i = 0; i < nspecies; i++) {
      for (j = 0; j < nelements; j++) {
	FormulaMatrix[j][i] = pub->FormulaMatrix[j][i];
      }
    }
  
    /*
     *  Copy over the species molecular weights
     */
    vcs_vdcopy(WtSpecies, pub->WtSpecies, nspecies);

    /*
     * Copy over the charges
     */
    vcs_vdcopy(Charge, pub->Charge, nspecies);

    /*
     * Malloc and Copy the VCS_SPECIES_THERMO structures
     * 
     */
    for (kspec = 0; kspec < nspecies; kspec++) {
      if (SpeciesThermo[kspec] != NULL) {
	delete SpeciesThermo[kspec];
      }
      VCS_SPECIES_THERMO *spf = pub->SpeciesThermo[kspec];
      SpeciesThermo[kspec] = spf->duplMyselfAsVCS_SPECIES_THERMO();
      if (SpeciesThermo[kspec] == NULL) {
	plogf(" duplMyselfAsVCS_SPECIES_THERMO returned an error!\n");
	return VCS_PUB_BAD;
      }
    }

    /*
     * Copy the species unknown type
     */
    vcs_icopy(VCS_DATA_PTR(SpeciesUnknownType), 
	      VCS_DATA_PTR(pub->SpeciesUnknownType), nspecies);

    /*
     *  iest => Do we have an initial estimate of the species mole numbers ?
     */
    iest = pub->iest;

    /*
     * w[] -> Copy the equilibrium mole number estimate if it exists.
     */
    if (pub->w.size() != 0) {
      vcs_vdcopy(soln, pub->w, nspecies);
    } else {
      iest = -1;
      vcs_dzero(VCS_DATA_PTR(soln), nspecies);
    }

    /*
     * Formulate the Goal Element Abundance Vector, gai[]
     */
    if (pub->gai.size() != 0) {
      for (i = 0; i < nelements; i++) gai[i] = pub->gai[i];
    } else {
      if (iest == 0) {
	for (j = 0; j < nelements; j++) {
	  gai[j] = 0.0;
	  for (kspec = 0; kspec < nspecies; kspec++) {
	    if (SpeciesUnknownType[kspec] != VCS_SPECIES_TYPE_INTERFACIALVOLTAGE) {
	      gai[j] += FormulaMatrix[j][kspec] * soln[kspec];
	    }
	  }
	}
      } else {
	plogf("%sElement Abundances, gai[], not specified\n", ser);
	return VCS_PUB_BAD;
      }
    }

    /*
     * zero out values that will be filled in later
     */
    /*
     * TPhMoles[]      -> Untouched here. These will be filled in vcs_prep.c
     * TPhMoles1[]
     * DelTPhMoles[]
     *
     *
     * T, Pres, copy over here
     */
    if (pub->T  > 0.0)  T    = pub->T;
    else                T    = 293.15;
    if (pub->Pres > 0.0) Pres = pub->Pres;
    else                 Pres = 1.0;
    /*
     *   TPhInertMoles[] -> must be copied over here
     */
    for (iph = 0; iph < nph; iph++) {
      Vphase = pub->VPhaseList[iph];
      TPhInertMoles[iph] = Vphase->TMolesInert;
    }

    /*
     *  if__ : Copy over the units for the chemical potential
     */
    m_VCS_UnitsFormat = pub->m_VCS_UnitsFormat;

    /*
     *      tolerance requirements -> copy them over here and later
     */
    tolmaj = pub->tolmaj;
    tolmin = pub->tolmin;
    tolmaj2 = 0.01 * tolmaj;
    tolmin2 = 0.01 * tolmin;
    /*
     * ind[] is an index variable that keep track of solution vector
     * rotations.
     */
    for (i = 0; i < nspecies; i++)   ind[i] = i;
    /*
     *   IndEl[] is an index variable that keep track of element vector
     *   rotations.
     */
    for (i = 0; i < nelements; i++)   IndEl[i] = i;
    /*
     *  ir[] -> will be done below once nc is defined.
     *  ic[] -> Define all species to be major species, initially.
     */
    for (i = 0; i < nspecies; i++) spStatus[i] = VCS_SPECIES_MAJOR;
    /*
     * PhaseID: Fill in the species to phase mapping
     *             -> Check for bad values at the same time.
     */
    if (pub->PhaseID.size() != 0) {
      std::vector<int> numPhSp(nph, 0);
      for (kspec = 0; kspec < nspecies; kspec++) {
	iph = pub->PhaseID[kspec];
	if (iph < 0 || iph >= nph) {
	  plogf("%sSpecies to Phase Mapping, PhaseID, has a bad value\n",
		 ser);
	  plogf("\tPhaseID[%d] = %d\n", kspec, iph);
	  plogf("\tAllowed values: 0 to %d\n", nph - 1);
	  return VCS_PUB_BAD;	    
	}
	PhaseID[kspec] = pub->PhaseID[kspec];
	indPhSp[kspec] = numPhSp[iph];
	numPhSp[iph]++;
      }
      for (iph = 0; iph < nph; iph++) {
	Vphase = pub->VPhaseList[iph];
	if (numPhSp[iph] != Vphase->NVolSpecies) {
	  plogf("%sNumber of species in phase %d, %s, doesn't match\n",
		 ser, iph, Vphase->PhaseName.c_str());
	  return VCS_PUB_BAD;
	}
      }
    } else {
      if (NPhase == 1) {
	for (kspec = 0; kspec < nspecies; kspec++) {
	  PhaseID[kspec] = 0;
	  indPhSp[kspec] = kspec;
	}
      } else {
	plogf("%sSpecies to Phase Mapping, PhaseID, is not defined\n", ser);
	return VCS_PUB_BAD;	 
      }
    }

    /*
     *  Copy over the element types
     */
    m_elType.resize(nelements, VCS_ELEM_TYPE_ABSPOS);
    ElActive.resize(nelements, 1);
  
    /*
     *      Copy over the element names
     */
    for (i = 0; i < nelements; i++) {
      ElName[i] = pub->ElName[i];
      m_elType[i] = pub->m_elType[i];
      ElActive[i] = pub->ElActive[i];
      if (!strncmp(ElName[i].c_str(), "cn_", 3)) {
	m_elType[i] = VCS_ELEM_TYPE_CHARGENEUTRALITY;
	if (pub->m_elType[i] != VCS_ELEM_TYPE_CHARGENEUTRALITY) {
	  plogf("we have an inconsistency!\n");
	  exit(-1);
	}
      }
    }
 
    /*
     *      Copy over the species names
     */
    for (i = 0; i < nspecies; i++) {
      SpName[i] = pub->SpName[i];
    }
    /*
     *  Copy over all of the phase information
     *  Use the object's assignment operator
     */
    for (iph = 0; iph < nph; iph++) {
      *(VPhaseList[iph]) = *(pub->VPhaseList[iph]);
      /*
       * Fix up the species thermo pointer in the vcs_SpeciesThermo object
       * It should point to the species thermo pointer in the private
       * data space.
       */
      Vphase = VPhaseList[iph];
      for (int k = 0; k < Vphase->NVolSpecies; k++) {
	vcs_SpeciesProperties *sProp = Vphase->ListSpeciesPtr[k];
	int kT = Vphase->IndSpecies[k];
	sProp->SpeciesThermo = SpeciesThermo[kT];
      }
    }

    /*
     * Specify the Activity Convention information
     */
    for (iph = 0; iph < nph; iph++) {
      Vphase = VPhaseList[iph];
      PhaseActConvention[iph] = Vphase->ActivityConvention;
      if (Vphase->ActivityConvention != 0) {
	/*
	 * We assume here that species 0 is the solvent.
	 * The solvent isn't on a unity activity basis
	 * The activity for the solvent assumes that the 
	 * it goes to one as the species mole fraction goes to
	 * one; i.e., it's really on a molarity framework.
	 * So SpecLnMnaught[iSolvent] = 0.0, and the
	 * loop below starts at 1, not 0.
	 */
	int iSolvent = Vphase->IndSpecies[0];
	double mnaught = WtSpecies[iSolvent] / 1000.;
	for (int k = 1; k < Vphase->NVolSpecies; k++) {
	  int kspec = Vphase->IndSpecies[k];
	  SpecActConvention[kspec] = Vphase->ActivityConvention;
	  SpecLnMnaught[kspec] = log(mnaught);
	}
      }
    }  
   
    /*
     *      Copy the title info
     */
    if (pub->Title.size() == 0) {
      Title = "Unspecified Problem Title";
    } else {
      Title = pub->Title;
    }

    /*
     *  Copy the volume info
     */
    Vol = pub->Vol; 
    if (VolPM.size() != 0) {
      vcs_dcopy(VCS_DATA_PTR(VolPM), VCS_DATA_PTR(pub->VolPM), nspecies);
    }
   
    /*
     *      Return the success flag
     */
    return VCS_SUCCESS;
  }
  /*****************************************************************************/
  /*****************************************************************************/
  /*****************************************************************************/
  // Specify the problem to be solved using VCS_PROB, incrementally
  /*
   *  Use the contents of the VCS_PROB to specify the contents of the
   *  private data, VCS_SOLVE.
   *
   *  It's assumed we are solving the same problem.
   *
   *  @param pub  Pointer to VCS_PROB that will be used to
   *              initialize the current equilibrium problem
   */
  int VCS_SOLVE::vcs_prob_specify(const VCS_PROB *pub) {
    int kspec, k, i, j, iph;
    string yo("vcs_prob_specify ERROR: ");
    int retn = VCS_SUCCESS;
    bool status_change = false;

    T = pub->T;
    Pres = pub->Pres;
    m_VCS_UnitsFormat = pub->m_VCS_UnitsFormat;
    iest = pub->iest;

    Vol = pub->Vol;

    tolmaj = pub->tolmaj;
    tolmin = pub->tolmin;
    tolmaj2 = 0.01 * tolmaj;
    tolmin2 = 0.01 * tolmin;

    for (kspec = 0; kspec < m_numSpeciesTot; ++kspec) {
      k = ind[kspec];
      soln[kspec] = pub->w[k];
      wt[kspec]   = pub->mf[k];
      m_gibbsSpecies[kspec]   = pub->m_gibbsSpecies[k];
    }

    /*
     * Transfer the element abundance goals to the solve object
     */
    for (i = 0; i < m_numElemConstraints; i++) {
      j = IndEl[i];
      gai[i] = pub->gai[j];
    }

    /*
     *  Try to do the best job at guessing at the title
     */
    if (pub->Title.size() == 0) {
      if (Title.size() == 0) {
	Title = "Unspecified Problem Title";
      }
    } else {
      Title = pub->Title;
    }

    /*
     *   Copy over the phase information.
     *      -> For each entry in the phase structure, determine
     *         if that entry can change from its initial value
     *         Either copy over the new value or create an error
     *         condition.
     */

    for (iph = 0; iph < NPhase; iph++) {
      vcs_VolPhase *vPhase = VPhaseList[iph];
      vcs_VolPhase *pub_phase_ptr = pub->VPhaseList[iph];

      if  (vPhase->VP_ID != pub_phase_ptr->VP_ID) {
	plogf("%sPhase numbers have changed:%d %d\n", yo.c_str(),
	       vPhase->VP_ID, pub_phase_ptr->VP_ID);
	retn = VCS_PUB_BAD;
      }

      if  (vPhase->SingleSpecies != pub_phase_ptr->SingleSpecies) {
	plogf("%sSingleSpecies value have changed:%d %d\n", yo.c_str(),
	       vPhase->SingleSpecies,
	       pub_phase_ptr->SingleSpecies);
	retn = VCS_PUB_BAD;
      }

      if  (vPhase->GasPhase != pub_phase_ptr->GasPhase) {
	plogf("%sGasPhase value have changed:%d %d\n", yo.c_str(),
	       vPhase->GasPhase,
	       pub_phase_ptr->GasPhase);
	retn = VCS_PUB_BAD;
      }

      vPhase->EqnState = pub_phase_ptr->EqnState;

      if  (vPhase->NVolSpecies != pub_phase_ptr->NVolSpecies) {
	plogf("%sNVolSpecies value have changed:%d %d\n", yo.c_str(),
	       vPhase->NVolSpecies,
	       pub_phase_ptr->NVolSpecies);
	retn = VCS_PUB_BAD;
      }

      if (vPhase->PhaseName == pub_phase_ptr->PhaseName) {
	plogf("%sPhaseName value have changed:%s %s\n", yo.c_str(),
	       vPhase->PhaseName.c_str(),
	       pub_phase_ptr->PhaseName.c_str());
	retn = VCS_PUB_BAD;
      }

      if (vPhase->TMolesInert != pub_phase_ptr->TMolesInert) {
	status_change = true;
      }
      /*
       * Copy over the number of inert moles if it has changed.
       */
      TPhInertMoles[iph] = pub_phase_ptr->TMolesInert;
      vPhase->TMolesInert = pub_phase_ptr->TMolesInert;
      if (TPhInertMoles[iph] > 0.0) {
	vPhase->Existence = 2;
	vPhase->SingleSpecies = FALSE;
      }

      /*
       * Copy over the interfacial potential
       */
      double phi = pub_phase_ptr->electricPotential();
      vPhase->setElectricPotential(phi);
    }
 

    if (status_change) vcs_SSPhase();
    /*
     *   Calculate the total number of moles in all phases.
     */
    vcs_tmoles();

    return retn;
  }
  /*****************************************************************************/
  /*****************************************************************************/
  /*****************************************************************************/
  // Transfer the results of the equilibrium calculation back to VCS_PROB
  /*
   *   The VCS_PUB structure is  returned to the user.
   *
   *  @param pub  Pointer to VCS_PROB that will get the results of the
   *              equilibrium calculation transfered to it. 
   */
  int VCS_SOLVE::vcs_prob_update(VCS_PROB *pub) {
    int i, j, k1, l;


    vcs_tmoles();
    Vol = vcs_VolTotal(T, Pres, VCS_DATA_PTR(soln), VCS_DATA_PTR(VolPM));

    for (i = 0; i < m_numSpeciesTot; ++i) {
      /*
       *         Find the index of I in the index vector, ind[]. 
       *         Call it K1 and continue. 
       */
      for (j = 0; j < m_numSpeciesTot; ++j) {
	l = ind[j];
	k1 = j;
	if (l == i) break;
      }
      /*
       * - Switch the species data back from K1 into I
       */
      if (pub->SpeciesUnknownType[i] != VCS_SPECIES_TYPE_INTERFACIALVOLTAGE) {
	pub->w[i]  =    soln[k1];
      } else {
	pub->w[i] = 0.0;
	plogf("voltage species = %g\n", soln[k1]);
      }
      pub->mf[i] =    wt[k1];
      pub->m_gibbsSpecies[i] = m_gibbsSpecies[k1];
      pub->VolPM[i] = VolPM[k1];
    } 
   
    pub->T    = T;
    pub->Pres = Pres;
    pub->Vol  = Vol;
    int kT = 0;
    for (int iph = 0; iph < pub->NPhase; iph++) {
      vcs_VolPhase *pubPhase = pub->VPhaseList[iph];
      vcs_VolPhase *vPhase = VPhaseList[iph];
      pubPhase->Existence = vPhase->Existence;
      // Note pubPhase is not the same as vPhase, since they contain
      // different indexing into the solution vector.
      // pubPhase->TMoles = vPhase->TMoles;
      pubPhase->TMolesInert = vPhase->TMolesInert;
      pubPhase->setTotalMoles(vPhase->TotalMoles());
      pubPhase->setElectricPotential(vPhase->electricPotential());
      double sumMoles = pubPhase->TMolesInert;
      pubPhase->setMoleFractions(VCS_DATA_PTR(vPhase->moleFractions()));
      for (int k = 0; k < pubPhase->NVolSpecies; k++) {
	kT = pubPhase->IndSpecies[k];
	//pubPhase->Xmol[k] = vPhase->Xmol[k];
	pubPhase->SS0ChemicalPotential[k] = vPhase->SS0ChemicalPotential[k];
	pubPhase->StarChemicalPotential[k] = vPhase->StarChemicalPotential[k];
	pubPhase->StarMolarVol[k] = vPhase->StarMolarVol[k];
	pubPhase->PartialMolarVol[k] = vPhase->PartialMolarVol[k];
	pubPhase->ActCoeff[k] = vPhase->ActCoeff[k];

	if (pubPhase->m_phiVarIndex == k) {
	  k1 = vPhase->IndSpecies[k];
	  double tmp = soln[k1];
	  if (! vcs_doubleEqual( 	pubPhase->electricPotential() , tmp)) { 
	    plogf("We have an inconsistency in voltage, %g, %g\n",
		   pubPhase->electricPotential(), tmp);
	    exit(-1);
	  }
	}


	if (! vcs_doubleEqual( pub->mf[kT], vPhase->molefraction(k))) {
	  plogf("We have an inconsistency in mole fraction, %g, %g\n",
		 pub->mf[kT], vPhase->molefraction(k));
	  exit(-1);
	}
	if (pubPhase->SpeciesUnknownType[k] != VCS_SPECIES_TYPE_INTERFACIALVOLTAGE) {
	  sumMoles +=  pub->w[kT];
	}
      }
      if (! vcs_doubleEqual(sumMoles, vPhase->TotalMoles())) {
      	plogf("We have an inconsistency in total moles, %g %g\n",
	       sumMoles, pubPhase->TotalMoles());
	exit(-1);
      }

    }

    pub->m_Iterations            = m_VCount->Its;
    pub->m_NumBasisOptimizations = m_VCount->Basis_Opts;
   
    return VCS_SUCCESS;
  }
  /*****************************************************************************/
  /*****************************************************************************/
  /*****************************************************************************/
  // Initialize the internal counters
  /*
   * Initialize the internal counters containing the subroutine call
   * values and times spent in the subroutines.
   *
   *  ifunc = 0     Initialize only those counters appropriate for the top of
   *                vcs_solve_TP().
   *        = 1     Initialize all counters.
   */
  void VCS_SOLVE::vcs_counters_init(int ifunc) {
    m_VCount->Its = 0;
    m_VCount->Basis_Opts = 0;
    m_VCount->Time_vcs_TP = 0.0;
    m_VCount->Time_basopt = 0.0;
    if (ifunc) {
      m_VCount->T_Its = 0;
      m_VCount->T_Basis_Opts = 0;
      m_VCount->T_Calls_Inest = 0;
      m_VCount->T_Calls_vcs_TP = 0;
      m_VCount->T_Time_vcs_TP = 0.0;
      m_VCount->T_Time_basopt = 0.0;
      m_VCount->T_Time_inest = 0.0;
      m_VCount->T_Time_vcs = 0.0;
    }
  }



}

