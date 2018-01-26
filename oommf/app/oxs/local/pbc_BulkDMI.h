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

#ifndef _OXS_BULKDMI
#define _OXS_BULKDMI

#include "atlas.h"
#include "key.h"
#include "energy.h"
#include "mesh.h"
#include "meshvalue.h"
#include "simstate.h"
#include "threevector.h"
#include "rectangularmesh.h"

class Oxs_PBC_BulkDMI:public Oxs_Energy {
private:
  OC_INT4m A_size;
  OC_REAL8m** D;
  Oxs_Key<Oxs_Atlas> atlaskey;  
  Oxs_OwnedPointer<Oxs_Atlas> atlas;
  mutable OC_INT4m mesh_id;
  mutable Oxs_MeshValue<OC_INT4m> region_id;

protected:
  virtual void GetEnergy(const Oxs_SimState& state,
			 Oxs_EnergyData& oed) const;

public:
  virtual const char* ClassName() const; // ClassName() is
  /// automatically generated by the OXS_EXT_REGISTER macro.
  Oxs_PBC_BulkDMI(const char* name,     // Child instance id
		   Oxs_Director* newdtr, // App director
		   const char* argstr);  // MIF input block parameters
  virtual ~Oxs_PBC_BulkDMI();
  virtual OC_BOOL Init();
};


#endif // _OXS_BULKDMI
