/*
 * opencog/embodiment/Control/PredicateUpdaters/NearPredicateUpdater.cc
 *
 * Copyright (C) 2002-2009 Novamente LLC
 * All Rights Reserved
 * Author(s): Ari Heljakka, Welter Luigi, Samir Araujo
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License v3 as
 * published by the Free Software Foundation and including the exceptions
 * at http://opencog.org/wiki/Licenses
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program; if not, write to:
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */


#include <opencog/atomspace/SimpleTruthValue.h>

#include "NearPredicateUpdater.h"
#include <opencog/embodiment/AtomSpaceExtensions/AtomSpaceUtil.h>
#include <opencog/spatial/Entity.h>

using namespace OperationalAvatarController;
using namespace opencog;
using namespace spatial;

NearPredicateUpdater::NearPredicateUpdater(AtomSpace &_atomSpace) :
        BasicPredicateUpdater(_atomSpace) {}

NearPredicateUpdater::~NearPredicateUpdater()
{
}

void NearPredicateUpdater::update(Handle object, Handle pet, unsigned long timestamp )
{
    // there is no map, no update is possible
    Handle spaceMapHandle = atomSpace.getSpaceServer().getLatestMapHandle();
    if (spaceMapHandle == Handle::UNDEFINED) {
        logger().warn( "NearPredicateUpdater::%s - No space map handle found!", __FUNCTION__);
        return;
    }
    const SpaceServer::SpaceMap& spaceMap = atomSpace.getSpaceServer().getLatestMap();

    std::vector<std::string> entities;
    spaceMap.findAllEntities(back_inserter(entities));

    logger().debug( "NearPredicateUpdater::%s - Processing timestamp '%lu'",
            __FUNCTION__, timestamp );
    if ( lastTimestamp != timestamp ) {
        lastTimestamp = timestamp;
        processedEntities.clear( );
    } // if
    
    const std::string& entityAId = atomSpace.getName( object );
    if ( processedEntities.find( entityAId ) != processedEntities.end( ) ) {
        return;
    } // if
    processedEntities.insert( entityAId );

    try {

        const spatial::EntityPtr& entityA = spaceMap.getEntity( entityAId );
    
        bool mapContainsEntity = spaceMap.containsObject( entityAId );

        for( unsigned int i = 0; i < entities.size( ); ++i ) {        
            const std::string& entityBId = entities[i];
            if ( processedEntities.find( entityBId ) != processedEntities.end( ) ) {
                continue;
            } // if
            Handle entityBHandle = getHandle( entityBId );

            if ( !mapContainsEntity ) {
                logger().debug( "NearPredicateUpdater::%s - Removing predicates "
                        "from '%s' and '%s'", __FUNCTION__,
                        entityAId.c_str( ), entityBId.c_str( ) );
                setPredicate( object, entityBHandle, "near", 0.0f );
                setPredicate( object, entityBHandle, "next", 0.0f );
            } else {
                const spatial::EntityPtr& entityB = spaceMap.getEntity( entityBId );
                double distance = entityA->distanceTo( *entityB );
                logger().debug( "NearPredicateUpdater::%s - Adding predicates "
                        "for '%s' and '%s'. distance '%f'", __FUNCTION__,
                        entityAId.c_str( ), entityBId.c_str( ), distance );

                spatial::math::Vector3 minCorner( spaceMap.xMin( ), spaceMap.yMin( ) );
                spatial::math::Vector3 maxCorner( spaceMap.xMax( ), spaceMap.yMax( ) );
            
                double mapDiagonal = ( maxCorner - minCorner ).length( );
            
                double nearDistance = spaceMap.getNearDistance( );
                double nextDistance = spaceMap.getNextDistance( );
            
                logger().debug( "NearPredicateUpdater::%s - nearDistance '%f'",
                                __FUNCTION__, nearDistance );

                setPredicate( object, entityBHandle, "near",
                        ( distance < nearDistance ) ? 1.0 : 0.0f );
                setPredicate( object, entityBHandle, "next",
                        ( distance < nextDistance ) ? 1.0 - (distance/nextDistance) : 0.0f );

                SimpleTruthValue tv(1.0 - (distance/mapDiagonal), 1);
                AtomSpaceUtil::setPredicateValue( atomSpace, "proximity", tv,
                        object, entityBHandle );
                AtomSpaceUtil::setPredicateValue( atomSpace, "proximity", tv,
                        entityBHandle, object );
            
            } // else
        } // for
    } catch( const opencog::NotFoundException& ex ) {
        logger().error( "NearPredicateUpdater::%s - Entity not found '%s'",
                        __FUNCTION__, ex.getMessage( ) );
    } // catch
        
}

void addRelationsToAtomSpace(std::list<Entity::SPATIAL_RELATION> relations, string entityA_id, string entityB_id, string entityC_id, AtomSpace& atomSpace);

//! @todo Doesn't process 3-object relations (i.e. BETWEEN). Should use more filtering to restrict its search (e.g. based on the
//! spatial grid, and/or apriori knowledge about which things can happen (e.g. "A is between B and C" can only happen if
//! "B is left of A" and "C is right of A".) Maybe respond only when an object has moved (or the agent - these relations are
//! evaluated from a certain perspective).
void NearPredicateUpdater::computeAllSpatialRelations(Handle observer, opencog::AtomSpace& atomSpace)
{
    HandleSeq resultingFrames;

    Handle objectA;
    Handle objectB;
    Handle objectC;

    // there is no map, no update is possible
    if (!atomSpace.getSpaceServer().isLatestMapValid()) {
        logger().warn( "%s - No space map handle found!", __FUNCTION__);
        return;
    }
    const SpaceServer::SpaceMap& spaceMap =
        atomSpace.getSpaceServer( ).getLatestMap( );

    double besideDistance = spaceMap.getNextDistance( );

    std::vector<std::string> entitiesA;
    std::vector<std::string> entitiesB;
    std::vector<std::string> entitiesC;

    spaceMap.getAllObjects( std::back_inserter( entitiesA ) );
    spaceMap.getAllObjects( std::back_inserter( entitiesB ) );
    spaceMap.getAllObjects( std::back_inserter( entitiesC ) );

    logger().debug( "%s - %d candidates for objectA. %d candidates for objectB. %d candidates for objectC",
                    __FUNCTION__, entitiesA.size( ), entitiesB.size( ), entitiesC.size( ) );

    int numRelations = 0;

    try {
        const spatial::EntityPtr& observerEntity = spaceMap.getEntity( atomSpace.getName( observer ) );

        unsigned int i, j, k;
        for( i = 0; i < entitiesA.size( ); ++i ) {
            const spatial::EntityPtr& entityA = spaceMap.getEntity( entitiesA[i] );
            for( j = 0; j < entitiesB.size( ); ++j ) {
                if ( entitiesA[i] == entitiesB[j] ) {
                    continue;
                } // if
                const spatial::EntityPtr& entityB = spaceMap.getEntity( entitiesB[j] );
                std::list<Entity::SPATIAL_RELATION> relations;

                // All size-2 and all size-3 relations
                relations = entityA->computeSpatialRelations( *observerEntity, besideDistance, *entityB );
                addRelationsToAtomSpace(relations, entitiesA[i], entitiesB[j], "", atomSpace);
                numRelations += relations.size();

//                for( k = 0; k < entitiesC.size( ); ++k ) {
//                    if ( entitiesA[i] == entitiesC[k] || entitiesB[j] == entitiesC[k] ) {
//                        continue;
//                    } // if
//                    const spatial::EntityPtr& entityC = spaceMap.getEntity( entitiesC[k] );
//                    relations = entityA->computeSpatialRelations( *observerEntity, besideDistance, *entityB, *entityC );
//                    addRelationsToAtomSpace(relations, entitiesA[i], entitiesB[j], entitiesC[k], atomSpace );
//                    numRelations += relations.size();
//                } // for
            } // for
        } // for
    } catch( const opencog::NotFoundException& ex ) {
        logger().error( "%s - %s", __FUNCTION__, ex.getMessage( ) );
        return;
    } // if

    logger().debug( "%s - Finished evaluating: %d spatial relations are true: %d combinations of 3 objects",
            __FUNCTION__, numRelations, entitiesA.size()*entitiesB.size()*entitiesC.size());

    return;
}

void addRelationsToAtomSpace(std::list<Entity::SPATIAL_RELATION> relations, std::string entityA_id, std::string entityB_id, std::string entityC_id, AtomSpace& atomSpace)
{
//    const SpaceServer::SpaceMap& spaceMap =
//        atomSpace.getSpaceServer().getLatestMap();
    HandleSeq justEntityA, justEntityB, justEntityC;
    atomSpace.getHandleSet(back_inserter(justEntityA), OBJECT_NODE, entityA_id, true);
    atomSpace.getHandleSet(back_inserter(justEntityB), OBJECT_NODE, entityB_id, true);

    Handle entityA, entityB, entityC;

    entityA = justEntityA[0];
    entityB = justEntityB[0];

    if (entityC_id.size()) {
        atomSpace.getHandleSet(back_inserter(justEntityC), OBJECT_NODE, entityC_id, true);
        entityC = justEntityC[0];
    } else {
        entityC == Handle::UNDEFINED;
    }

    SimpleTruthValue tv( 1, 1 );
    foreach (Entity::SPATIAL_RELATION rel, relations) {
        string predicateName = Entity::spatialRelationToString(rel);
        if (entityC == Handle::UNDEFINED)
            AtomSpaceUtil::setPredicateValue( atomSpace, predicateName, tv, entityA, entityB );
        else
            AtomSpaceUtil::setPredicateValue( atomSpace, predicateName, tv, entityA, entityB, entityC );
    }
}

void NearPredicateUpdater::setPredicate( const Handle& entityA, const Handle& entityB, const std::string& predicateName, float mean )
{
    SimpleTruthValue tv( mean, 1 );
    AtomSpaceUtil::setPredicateValue( atomSpace, predicateName, tv, entityA, entityB );
    AtomSpaceUtil::setPredicateValue( atomSpace, predicateName, tv, entityB, entityA );
}
