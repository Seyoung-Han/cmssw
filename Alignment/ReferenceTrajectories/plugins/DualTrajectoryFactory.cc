#include "FWCore/Framework/interface/ESHandle.h"
#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "MagneticField/Records/interface/IdealMagneticFieldRecord.h" 
#include "Alignment/ReferenceTrajectories/interface/TrajectoryFactoryPlugin.h"
#include "TrackingTools/GeomPropagators/interface/AnalyticalPropagator.h"

#include <algorithm>

#include "Alignment/ReferenceTrajectories/interface/DualReferenceTrajectory.h" 
#include "Alignment/ReferenceTrajectories/interface/TrajectoryFactoryBase.h"

/// A factory that produces instances of class ReferenceTrajectory from a given TrajTrackPairCollection.


class DualTrajectoryFactory : public TrajectoryFactoryBase
{
public:
  DualTrajectoryFactory(const edm::ParameterSet &config);
  virtual ~DualTrajectoryFactory();

  /// Produce the reference trajectories.
  virtual const ReferenceTrajectoryCollection trajectories(const edm::EventSetup  &setup,
							   const ConstTrajTrackPairCollection &tracks,
							   const reco::BeamSpot &beamSpot) const;

  virtual const ReferenceTrajectoryCollection trajectories(const edm::EventSetup &setup,
							   const ConstTrajTrackPairCollection &tracks,
							   const ExternalPredictionCollection &external,
							   const reco::BeamSpot &beamSpot) const;

  virtual DualTrajectoryFactory* clone() const { return new DualTrajectoryFactory(*this); }

protected:
  struct DualTrajectoryInput
  {
    TrajectoryStateOnSurface refTsos;
    TransientTrackingRecHit::ConstRecHitContainer fwdRecHits;
    TransientTrackingRecHit::ConstRecHitContainer bwdRecHits;
  };

  const DualTrajectoryInput referenceStateAndRecHits(const ConstTrajTrackPair &track) const;

  const TrajectoryStateOnSurface propagateExternal(const TrajectoryStateOnSurface &external,
						   const Surface &surface,
						   const MagneticField *magField) const;

  double theMass;
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

DualTrajectoryFactory::DualTrajectoryFactory( const edm::ParameterSet & config ) :
  TrajectoryFactoryBase( config )
{
  theMass = config.getParameter< double >( "ParticleMass" );
}

 
DualTrajectoryFactory::~DualTrajectoryFactory( void ) {}


const DualTrajectoryFactory::ReferenceTrajectoryCollection
DualTrajectoryFactory::trajectories(const edm::EventSetup &setup,
				    const ConstTrajTrackPairCollection &tracks,
				    const reco::BeamSpot &beamSpot) const
{
  ReferenceTrajectoryCollection trajectories;

  edm::ESHandle< MagneticField > magneticField;
  setup.get< IdealMagneticFieldRecord >().get( magneticField );

  ConstTrajTrackPairCollection::const_iterator itTracks = tracks.begin();

  while ( itTracks != tracks.end() )
  { 
    const DualTrajectoryInput input = this->referenceStateAndRecHits( *itTracks );
    // Check input: If all hits were rejected, the TSOS is initialized as invalid.
    if ( input.refTsos.isValid() )
    {
      ReferenceTrajectoryPtr ptr( new DualReferenceTrajectory( input.refTsos,
							       input.fwdRecHits,
							       input.bwdRecHits,
							       magneticField.product(),
							       materialEffects(),
							       propagationDirection(),
							       theMass,
							       theUseBeamSpot, beamSpot ) );
      trajectories.push_back( ptr );
    }

    ++itTracks;
  }

  return trajectories;
}

const DualTrajectoryFactory::ReferenceTrajectoryCollection
DualTrajectoryFactory::trajectories(const edm::EventSetup &setup,
				    const ConstTrajTrackPairCollection &tracks,
				    const ExternalPredictionCollection &external,
				    const reco::BeamSpot &beamSpot) const
{
  ReferenceTrajectoryCollection trajectories;

  if ( tracks.size() != external.size() )
  {
    edm::LogInfo("ReferenceTrajectories") << "@SUB=DualTrajectoryFactory::trajectories"
					  << "Inconsistent input:\n"
					  << "\tnumber of tracks = " << tracks.size()
					  << "\tnumber of external predictions = " << external.size();
    return trajectories;
  }

  edm::ESHandle< MagneticField > magneticField;
  setup.get< IdealMagneticFieldRecord >().get( magneticField );

  ConstTrajTrackPairCollection::const_iterator itTracks = tracks.begin();
  ExternalPredictionCollection::const_iterator itExternal = external.begin();

  while ( itTracks != tracks.end() )
  {
    const DualTrajectoryInput input = referenceStateAndRecHits( *itTracks );
    // Check input: If all hits were rejected, the TSOS is initialized as invalid.
    if ( input.refTsos.isValid() )
    {
      if ( (*itExternal).isValid() )
      {
	TrajectoryStateOnSurface propExternal =
	  propagateExternal( *itExternal, input.refTsos.surface(), magneticField.product() );

	if ( !propExternal.isValid() ) continue;

	// set the flag for reversing the RecHits to false, since they are already in the correct order.
	ReferenceTrajectoryPtr ptr( new DualReferenceTrajectory( propExternal,
								 input.fwdRecHits,
								 input.bwdRecHits,
								 magneticField.product(),
								 materialEffects(),
								 propagationDirection(),
								 theMass, 
								 theUseBeamSpot, beamSpot ) );

	AlgebraicSymMatrix externalParamErrors( asHepMatrix<5>( propExternal.localError().matrix() ) );
	ptr->setParameterErrors( externalParamErrors );
	trajectories.push_back( ptr );
      }
      else
      {
	ReferenceTrajectoryPtr ptr( new DualReferenceTrajectory( input.refTsos,
								 input.fwdRecHits,
								 input.bwdRecHits,
								 magneticField.product(),
								 materialEffects(),
								 propagationDirection(),
								 theMass, 
								 theUseBeamSpot, beamSpot ) );
	trajectories.push_back( ptr );
      }
    }

    ++itTracks;
    ++itExternal;
  }

  return trajectories;
}


const DualTrajectoryFactory::DualTrajectoryInput
DualTrajectoryFactory::referenceStateAndRecHits( const ConstTrajTrackPair& track ) const
{
  DualTrajectoryInput input;
 
  // get the trajectory measurements in the correct order, i.e. reverse if needed
  Trajectory::DataContainer allTrajMeas = this->orderedTrajectoryMeasurements( *track.first );
  Trajectory::DataContainer usedTrajMeas;
  Trajectory::DataContainer::iterator itM;
  // get all relevant trajectory measurements
  for ( itM = allTrajMeas.begin(); itM != allTrajMeas.end(); itM++ )
  {
    if ( useRecHit( ( *itM ).recHit() ) ) usedTrajMeas.push_back( *itM );
  }

  unsigned int iMeas = 0;
  unsigned int nMeas = usedTrajMeas.size();
  unsigned int nRefStateMeas = nMeas/2;
  // get the valid RecHits
  for ( itM = usedTrajMeas.begin(); itM != usedTrajMeas.end(); itM++, iMeas++ )
  {
    TransientTrackingRecHit::ConstRecHitPointer aRecHit = ( *itM ).recHit();

    if ( iMeas < nRefStateMeas ) {
      input.bwdRecHits.push_back( aRecHit );
    } else if ( iMeas > nRefStateMeas ) {
      input.fwdRecHits.push_back( aRecHit );
    } else { // iMeas == nRefStateMeas
      if ( ( *itM ).updatedState().isValid() )
      {
	input.refTsos = ( *itM ).updatedState();
	input.bwdRecHits.push_back( aRecHit );
	input.fwdRecHits.push_back( aRecHit );
      } else {
	// if the tsos of the middle hit is not valid, try the next one ...
	nRefStateMeas++;
	input.bwdRecHits.push_back( aRecHit );
      }
    }
  }

  // bring input.fwdRecHits into correct order
  std::reverse( input.bwdRecHits.begin(), input.bwdRecHits.end() );

  return input;
}

const TrajectoryStateOnSurface
DualTrajectoryFactory::propagateExternal( const TrajectoryStateOnSurface& external,
					  const Surface& surface,
					  const MagneticField* magField ) const
{
  AnalyticalPropagator propagator( magField, anyDirection );
  const std::pair< TrajectoryStateOnSurface, double > tsosWithPath =
    propagator.propagateWithPath( external, surface );
  return tsosWithPath.first;
}


DEFINE_EDM_PLUGIN( TrajectoryFactoryPlugin, DualTrajectoryFactory, "DualTrajectoryFactory" );
