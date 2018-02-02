/* FILE: PBC_BulkDMI.cc
 *
 * Bulk Dzyaloshinskii-Moriya energy:
 *
 * $w_\text{dmi} = D\mathbf{m} \cdot (\nabla \times \mathbf{m})$
 *
 * Modification by Marijan Beg, Ryan A. Pepper, and Hans Fangohr (University of Southampton) of Oxs_DMexchange6ngbr.h [1] - January 2017
 *
 * Developed as a part of OpenDreamKit Horizon 2020 European Research Infrastructure
 * project (676541), and the EPSRC Programme grant on Skyrmionics (EP/N032128/1).
 *
 * [1] Rohart, S., & Thiaville, A. Physical Review B, 88, 184422 (2013).
 *
*/

#include <string>

#include "atlas.h"
#include "nb.h"
#include "key.h"
#include "director.h"
#include "mesh.h"
#include "meshvalue.h"
#include "simstate.h"
#include "threevector.h"
#include "rectangularmesh.h"
#include "pbc_BulkDMI.h"
#include "energy.h"		// Needed to make MSVC++ 5 happy

OC_USE_STRING;

// Oxs_Ext registration support
OXS_EXT_REGISTER(Oxs_PBC_BulkDMI);

/* End includes */


// Constructor
Oxs_PBC_BulkDMI::Oxs_PBC_BulkDMI(
  const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr)   // MIF input block parameters
  : Oxs_Energy(name,newdtr,argstr),
    A_size(0), D(NULL), mesh_id(0)
{
  // Process arguments
  OXS_GET_INIT_EXT_OBJECT("atlas",Oxs_Atlas,atlas);
  atlaskey.Set(atlas.GetPtr());
  /// Dependency lock is held until *this is deleted.

  // Check for optional default_D parameter; default is 0.
  OC_REAL8m default_D = GetRealInitValue("default_D",0.0);

  // Allocate A matrix.  Because raw pointers are used, a memory
  // leak will occur if an exception is thrown inside this constructor.
  A_size = atlas->GetRegionCount();
  if(A_size<1) {
    String msg = String("Oxs_Atlas object ")
      + atlas->InstanceName()
      + String(" must contain at least one region.");

    throw Oxs_Ext::Error(msg.c_str());
  }
  D = new OC_REAL8m*[A_size];
  D[0] = new OC_REAL8m[A_size*A_size];
  OC_INDEX i;
  for(i=1;i<A_size;i++) D[i] = D[i-1] + A_size;
  for(i=0;i<A_size*A_size;i++) D[0][i] = default_D;

  // Fill D matrix
  vector<String> params;
  FindRequiredInitValue("D",params);
  if(params.empty()) {
    throw Oxs_Ext::Error(this,"Empty parameter list for key \"D\"");
  }
  if(params.size()%3!=0) {
      char buf[512];
      Oc_Snprintf(buf,sizeof(buf),
		  "Number of elements in D sub-list must be"
		  " divisible by 3"
		  " (actual sub-list size: %u)",
		  (unsigned int)params.size());
      throw Oxs_Ext::Error(this,buf);
  }
  for(i=0;i<params.size();i+=3) {
    OC_INT4m i1 = atlas->GetRegionId(params[i]);
    OC_INT4m i2 = atlas->GetRegionId(params[i+1]);
    if(i1<0 || i2<0) {
      char buf[4096];
      char* cptr=buf;
      if(i1<0) {
	Oc_Snprintf(buf,sizeof(buf),
		    "First entry in D[%u] sub-list, \"%s\","
		    " is not a known region in atlas \"%s\".  ",
		    i/3,params[i].c_str(),atlas->InstanceName());
	cptr += strlen(buf);
      }
      if(i2<0) {
	Oc_Snprintf(cptr,sizeof(buf)-(cptr-buf),
		    "Second entry in D[%u] sub-list, \"%s\","
		    " is not a known region in atlas \"%s\".  ",
		    i/3,params[i+1].c_str(),atlas->InstanceName());
      }
      String msg = String(buf);
      msg += String("Known regions:");
      vector<String> regions;
      atlas->GetRegionList(regions);
      for(unsigned int j=0;j<regions.size();++j) {
	msg += String(" \n");
	msg += regions[j];
      }
      throw Oxs_Ext::Error(this,msg);
    }
    OC_BOOL err;
    OC_REAL8m Dpair = Nb_Atof(params[i+2].c_str(),err);
    if(err) {
      char buf[4096];
      Oc_Snprintf(buf,sizeof(buf),
		  "Third entry in D[%u] sub-list, \"%s\","
		  " is not a valid floating point number.",
		  i/3,params[i+2].c_str());
      throw Oxs_Ext::Error(this,buf);
    }
    D[i1][i2]=Dpair;
    D[i2][i1]=Dpair; // D should be symmetric
  }
  DeleteInitValue("D");

  VerifyAllInitArgsUsed();
}

Oxs_PBC_BulkDMI::~Oxs_PBC_BulkDMI()
{
  if(A_size>0 && D!=NULL) {
    delete[] D[0];
    delete[] D;
  }
}

OC_BOOL Oxs_PBC_BulkDMI::Init()
{
  mesh_id = 0;
  region_id.Release();
  return Oxs_Energy::Init();
}

void Oxs_PBC_BulkDMI::GetEnergy
(const Oxs_SimState& state,
 Oxs_EnergyData& oed
 ) const
{
  // See if mesh and/or atlas has changed.
  if(mesh_id !=  state.mesh->Id() || !atlaskey.SameState()) {
    // Setup region mapping
    mesh_id = 0; // Safety
    OC_INDEX size = state.mesh->Size();
    region_id.AdjustSize(state.mesh);
    ThreeVector location;
    for(OC_INDEX i=0;i<size;i++) {
      state.mesh->Center(i,location);
      if((region_id[i] = atlas->GetRegionId(location))<0) {
	String msg = String("Import mesh to Oxs_PBC_BulkDMI::GetEnergy()"
                            " routine of object ")
          + String(InstanceName())
	  + String(" has points outside atlas ")
	  + String(atlas->InstanceName());
	throw Oxs_Ext::Error(msg.c_str());
      }
    }
    mesh_id = state.mesh->Id();
  }
  const Oxs_MeshValue<ThreeVector>& spin = state.spin;
  const Oxs_MeshValue<OC_REAL8m>& Ms_inverse = *(state.Ms_inverse);

  // Use supplied buffer space, and reflect that use in oed.
  oed.energy = oed.energy_buffer;
  oed.field = oed.field_buffer;
  Oxs_MeshValue<OC_REAL8m>& energy = *oed.energy_buffer;
  Oxs_MeshValue<ThreeVector>& field = *oed.field_buffer;

  const Oxs_PeriodicRectangularMesh* mesh
    = dynamic_cast<const Oxs_PeriodicRectangularMesh*>(state.mesh);
  if(mesh==NULL) {
    String msg = String("Import mesh to Oxs_PBC_BulkDMI::GetEnergy()"
                        " routine of object ")
      + String(InstanceName())
      + String(" is not an Oxs_PeriodicRectangularMesh object.");
    throw Oxs_Ext::Error(msg.c_str());
  }

  OC_INDEX xdim = mesh->DimX();
  OC_INDEX ydim = mesh->DimY();
  OC_INDEX zdim = mesh->DimZ();
  OC_INDEX xydim = xdim*ydim;

  OC_REAL8m wgtx = 1.0/(mesh->EdgeLengthX());
  OC_REAL8m wgty = 1.0/(mesh->EdgeLengthY());
  OC_REAL8m wgtz = 1.0/(mesh->EdgeLengthZ());

  OC_REAL8m hcoef = -2/MU0;

  for(OC_INDEX z=0;z<zdim;z++) {
    for(OC_INDEX y=0;y<ydim;y++) {
      for(OC_INDEX x=0;x<xdim;x++) {
	OC_INDEX i = mesh->Index(x,y,z); // Get base linear address
	ThreeVector base = spin[i];
	OC_REAL8m Msii = Ms_inverse[i];
	if(Msii == 0.0) {
	  energy[i]=0.0;
	  field[i].Set(0.,0.,0.);
	  continue;
	}
	OC_REAL8m* Drow = D[region_id[i]];
	ThreeVector sum(0.,0.,0.);
    OC_INDEX j;

    {
	if(x > 0) j = i - 1;   // j = mesh->Index(x-1,y,z)
    else      j = i - 1 + xdim; // x == 0, j = Index(xdim-1,y,z);
	OC_REAL8m Dpair = Drow[region_id[j]];
	ThreeVector uij(-1.,0.,0);
	if(Ms_inverse[j]!=0.0) sum += 0.5*Dpair*wgtx*(spin[j] ^ uij);
    }

    {
	if(y > 0) j = i-xdim;
    else      j = i - xdim + xydim;
	OC_REAL8m Dpair = Drow[region_id[j]];
	ThreeVector uij(0.,-1.,0);
	if(Ms_inverse[j]!=0.0) sum += 0.5*Dpair*wgty*(spin[j] ^ uij);
    }

    // No PBC in the z direction
	if(z > 0) {
	  OC_INDEX j = i-xydim;
	  OC_REAL8m Dpair = Drow[region_id[j]];
	  ThreeVector uij(0.,0.,-1.);
	  if(Ms_inverse[j]!=0.0) sum += 0.5*Dpair*wgtz*(spin[j] ^ uij);
	}

    {
	if(x < xdim-1) j = i + 1;
	else           j = i + 1 - xdim;
	OC_REAL8m Dpair = Drow[region_id[j]];
	ThreeVector uij(1.,0.,0);
	if(Ms_inverse[j]!=0.0) sum += 0.5*Dpair*wgtx*(spin[j] ^ uij);
    }

    {
	if(y < ydim-1) j = i + xdim;
	else           j = i + xdim - xydim;
	OC_REAL8m Dpair = Drow[region_id[j]];
	ThreeVector uij(0.,1.,0);
	if(Ms_inverse[j]!=0.0) sum += 0.5*Dpair*wgty*(spin[j] ^ uij);
    }

    // No PBC in the z direction
	if(z < zdim-1) {
	  OC_INDEX j = i+xydim;
	  OC_REAL8m Dpair = Drow[region_id[j]];
	  ThreeVector uij(0.,0.,1.);
	  if(Ms_inverse[j]!=0.0) sum += 0.5*Dpair*wgtz*(spin[j] ^ uij);
	}
	
	field[i] = (hcoef*Msii) * sum;
	energy[i] = (sum * base);
      }
    }
  }
}