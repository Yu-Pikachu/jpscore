/**
 * \file        FloorfieldViaFM.cpp
 * \date        Mar 05, 2015
 * \version     N/A (v0.6)
 * \copyright   <2009-2014> Forschungszentrum Jülich GmbH. All rights reserved.
 *
 * \section License
 * This file is part of JuPedSim.
 *
 * JuPedSim is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * JuPedSim is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with JuPedSim. If not, see <http://www.gnu.org/licenses/>.
 *
 * \section Description
 * Implementation of classes for ...
 *
 *
 **/
#define TESTING
#define GEO_UP_SCALE 1
#include "FloorfieldViaFM.h"
#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <limits>
#include <chrono>

#ifdef _OPENMP
#include <omp.h>
#else
#define omp_get_thread_num() 0
#define omp_get_max_threads()  1
#endif

FloorfieldViaFM::FloorfieldViaFM()
{
    //ctor (very ugly)
    //std::cerr << "The defaultconsturctor FloorfieldViaFM should not be called!" << std::endl;
}

FloorfieldViaFM::~FloorfieldViaFM()
{
    //dtor
    delete grid;
    if (flag) delete[] flag;
    if (dist2Wall) delete[] dist2Wall;
    if (speedInitial) delete[] speedInitial;
    if (modifiedspeed) delete[] modifiedspeed;
    //if (cost) delete[] cost;
    //if (neggrad) delete[] neggrad;
    if (dirToWall) delete[] dirToWall;
    if (trialfield) delete[] trialfield;
    for ( const auto& goalid : goalcostmap) {
        if (goalid.second) delete[] goalid.second;
    }
    for ( const auto& id : costmap) {
        //if (id.first == -1) continue;
        if (id.second) delete[] id.second;
        if (neggradmap.at(id.first)) delete[] neggradmap.at(id.first);
        //map will be deleted by itself
    }

}

FloorfieldViaFM::FloorfieldViaFM(const Building* const buildingArg, const double hxArg, const double hyArg,
                                 const double wallAvoidDistance, const bool useDistancefield, const std::string& filename) {
    //ctor
    threshold = -1; //negative value means: ignore threshold
    threshold = wallAvoidDistance;
    building = buildingArg;

    if (hxArg != hyArg) std::cerr << "ERROR: hx != hy <=========";
    //parse building and create list of walls/obstacles (find xmin xmax, ymin, ymax, and add border?)
    Log->Write("INFO: \tStart Parsing: Building");
    parseBuilding(buildingArg, hxArg, hyArg);
    Log->Write("INFO: \tFinished Parsing: Building");
    //testoutput("AALineScan.vtk", "AALineScan.txt", dist2Wall);

    prepareForDistanceFieldCalculation(wall);
    Log->Write("INFO: \tGrid initialized: Walls");

    calculateDistanceField(-1.); //negative threshold is ignored, so all distances get calculated. this is important since distances is used for slowdown/redirect
    Log->Write("INFO: \tGrid initialized: Walldistances");

    setSpeed(useDistancefield); //use distance2Wall
    Log->Write("INFO: \tGrid initialized: Speed");
    calculateFloorfield(cost, neggrad);
    //writing FF-file disabled, we will not revive it ( @todo: argraf )
}

FloorfieldViaFM::FloorfieldViaFM(const std::string& filename) {

//                    FileHeaderExample: (GEO_UP_SCALE is assumed to be 1.0)
//                    # vtk DataFile Version 3.0
//                    Testdata: Fast Marching: Test:
//                    ASCII
//                    DATASET STRUCTURED_POINTS
//                    DIMENSIONS 322 226 1
//                    ORIGIN 50 90 0
//                    SPACING 0.062500 0.062500 1
//                    POINT_DATA 72772
//                    SCALARS Cost float 1
//                    LOOKUP_TABLE default
//                    0.505725
//                    0.505725
//                    0.505725
//                    ...

// comments show lineformat in .vtk file (below)
    std::ifstream file(filename);
    std::string line;

    std::getline(file, line); //# vtk DataFile Version 3.0
    std::getline(file, line); //Testdata: Fast Marching: Test:
    std::getline(file, line); //ASCII
    std::getline(file, line); //DATASET STRUCTURED_POINTS
    std::getline(file, line); //DIMENSIONS {x} {y} {z}

    std::stringstream inputline(line);

    long int iMax, jMax, c;
    std::string dummy;
    double fdummy;
    long int nPoints;
    double xMin;
    double yMin;
    double xMax;
    double yMax;
    double hx;
    double hy;


    inputline >> dummy >> iMax >> jMax >> c ;

    std::getline(file, line); //ORIGIN x y z
    inputline.str("");
    inputline.clear();
    inputline << line;
    inputline >> dummy >> xMin >> yMin >> c;

    std::getline(file, line); //SPACING 0.062500 0.062500 1
    inputline.str("");
    inputline.clear();
    inputline << line;
    inputline >> dummy >> hx >> hy >> c;
    xMax = xMin + hx*iMax;
    yMax = yMin + hy*jMax;

    std::getline(file, line); //POINT_DATA 72772
    inputline.str("");
    inputline.clear();
    inputline.flush();
    inputline << line;
    inputline >> dummy >> nPoints;

    //create Rect Grid
    grid = new RectGrid(nPoints, xMin, yMin, xMax, yMax, hx, hy, iMax, jMax, true);

    //create arrays
    flag = new int[nPoints];                  //flag:( 0 = unknown, 1 = singel, 2 = double, 3 = final, -7 = outside)
    dist2Wall = new double[nPoints];
    speedInitial = new double[nPoints];
    cost = new double[nPoints];
    neggrad = new Point[nPoints];
    dirToWall = new Point[nPoints];
    trialfield = new Trial[nPoints];                 //created with other arrays, but not initialized yet

    std::getline(file, line);   //SCALARS Cost float 1
    std::getline(file, line);   //LOOKUP_TABLE default

    for (long int i = 0; i < nPoints; ++i) {
        std::getline(file, line);
        inputline.str("");
        inputline << line;
        inputline >> dist2Wall[i];  //0.505725
        //std::cerr << dist2Wall[i] << std::endl;
        inputline.clear();
    }

    std::getline(file, line);       //VECTORS Gradient float

    for (long int i = 0; i < nPoints; ++i) {
        std::getline(file, line);
        inputline.str("");
        inputline << line;
        inputline >> neggrad[i]._x >> neggrad[i]._y >> fdummy;  //0.989337 7.88255 0.0
        inputline.clear();
    }

    std::getline(file, line);       //VECTORS Gradient float

    for (long int i = 0; i < nPoints; ++i) {
        std::getline(file, line);
        inputline.str("");
        inputline << line;
        inputline >> dirToWall[i]._x >> dirToWall[i]._y >> fdummy;  //0.989337 7.88255 0.0
        inputline.clear();
    }
    file.close();
}

void FloorfieldViaFM::getDirectionAt(const Point& position, Point& direction){
    long int key = grid->getKeyAtPoint(position);
    direction._x = (neggrad[key]._x);
    direction._y = (neggrad[key]._y);
}

void FloorfieldViaFM::getDirectionToDestination(Pedestrian* ped, Point& direction){
    const Point& position = ped->GetPos();
    int destID = ped->GetExitIndex();
    long int key = grid->getKeyAtPoint(position);
    getDirectionToUID(destID, key, direction);
}

void FloorfieldViaFM::getDirectionToUID(int destID, const long int key, Point& direction) {
    //what if goal == -1, meaning closest exit... is GetExitIndex then -1? NO... ExitIndex is UID, given by router
    //if (ped->GetFinalDestination() == -1) /*go to closest exit*/ destID = -1;

    if ((key < 0) || (key >= grid->GetnPoints())) { // @todo: ar.graf: this check in a #ifdef-block?
        Log->Write("ERROR: \t Floorfield tried to access a key out of grid!");
        direction._x = 0.;
        direction._y = 0.;
        return;
    }
    Point* localneggradptr;
    double* localcostptr;
    #pragma omp critical
    {
        if (neggradmap.count(destID) == 0) {
            //check, if distID is in this grid
            Hline* destLine = building->GetTransOrCrossByUID(destID);
            Point A = destLine->GetPoint1();
            Point B = destLine->GetPoint2();
            if (!(grid->includesPoint(A)) || !(grid->includesPoint(B))) {
                Log->Write("ERROR: \t Destination ID %d is not in grid!", destID);
                direction._x = direction._y = 0.;
                return;
            }
            neggradmap.emplace(destID, nullptr);
            costmap.emplace(destID, nullptr);
        }
        localneggradptr = neggradmap.at(destID);
        localcostptr = costmap.at(destID);
        if (localneggradptr == nullptr) {
                //create floorfield (remove mapentry with nullptr, allocate memory, add mapentry, create ff)
                localcostptr =    new double[grid->GetnPoints()];
                localneggradptr = new Point[grid->GetnPoints()];
                neggradmap.erase(destID);
                neggradmap.emplace(destID, localneggradptr);
                costmap.erase(destID);
                costmap.emplace(destID, localcostptr);
                //create ff (prepare Trial-mechanic, then calc)
                for (long int i = 0; i < grid->GetnPoints(); ++i) {
                    //set Trialptr to fieldelements
                    trialfield[i].cost = localcostptr + i;
                    trialfield[i].neggrad = localneggradptr + i;
                    trialfield[i].father = nullptr;
                    trialfield[i].child = nullptr;
                }
                clearAndPrepareForFloorfieldReCalc(localcostptr);
                std::vector<Line> localline = {Line((Line) *(building->GetTransOrCrossByUID(destID)))};
                setNewGoalAfterTheClear(localcostptr, localline);

                calculateFloorfield(localcostptr, localneggradptr);
        }
    }
    direction._x = (localneggradptr[key]._x);
    direction._y = (localneggradptr[key]._y);
}

void FloorfieldViaFM::getDirectionToFinalDestination(Pedestrian* ped, Point& direction){
    const Point& position = ped->GetPos();
    const int goalID = ped->GetFinalDestination();
    long int key = grid->getKeyAtPoint(position);
    //we assume, only ground level (planeEquation[2] == 0), which is C == 0, allows to exit to goal
    if ((goalID == -1) && (building->GetSubRoomByUID(ped->GetSubRoomUID())->GetPlaneEquation()[2] == 0.)) {
        direction._x = neggrad[key]._x;
        direction._y = neggrad[key]._y;
    }
    createLineToGoalID(goalID);

    getDirectionToUID(goalToLineUIDmap.at(goalID), key, direction);
}

void FloorfieldViaFM::createLineToGoalID(const int goalID)
{
    Point* localneggradptr;
    double* localcostptr;
    if (goalID < 0) {
        Log->Write("WARNING: \t goalID was negative in FloorfieldViaFM::createLineToGoalID");
        return;
    }
    if (!building->GetFinalGoal(goalID)) {
        Log->Write("WARNING: \t goalID was unknown in FloorfieldViaFM::createLineToGoalID");
        return;
    }
#pragma omp critical
    {
        if (goalcostmap.count(goalID) == 0) { //no entry for goalcostmap, so we need to calc FF
            goalcostmap.emplace(goalID, nullptr);
            goalneggradmap.emplace(goalID, nullptr);
            goalToLineUIDmap.emplace(goalID, -1);
            goalToLineUIDmap2.emplace(goalID, -1);
            goalToLineUIDmap3.emplace(goalID, -1);
        }
        localneggradptr = goalneggradmap.at(goalID);
        localcostptr = goalcostmap.at(goalID);
        if (localneggradptr == nullptr) {
            //create floorfield (remove mapentry with nullptr, allocate memory, add mapentry, create ff)
            localcostptr =    new double[grid->GetnPoints()];
            localneggradptr = new Point[grid->GetnPoints()];
            goalneggradmap.erase(goalID);
            goalneggradmap.emplace(goalID, localneggradptr);
            goalcostmap.erase(goalID);
            goalcostmap.emplace(goalID, localcostptr);
            //create ff (prepare Trial-mechanic, then calc)
            for (long int i = 0; i < grid->GetnPoints(); ++i) {
                //set Trialptr to fieldelements
                trialfield[i].cost = localcostptr + i;
                trialfield[i].neggrad = localneggradptr + i;
                trialfield[i].father = nullptr;
                trialfield[i].child = nullptr;
            }
            clearAndPrepareForFloorfieldReCalc(localcostptr);

            //get all lines/walls of goalID
            vector<Line> localline;
            const std::map<int, Goal*>& allgoals = building->GetAllGoals();
            vector<Wall> localwalls = allgoals.at(goalID)->GetAllWalls();

            double xMin = grid->GetxMin();
            double xMax = grid->GetxMax();

            double yMin = grid->GetyMin();
            double yMax = grid->GetyMax();

            for (const auto& iwall:localwalls) {
                const Point& a = iwall.GetPoint1();
                const Point& b = iwall.GetPoint2();
                if (
                      (a._x >= xMin) && (a._x <= xMax)
                    &&(a._y >= yMin) && (a._y <= yMax)
                    &&(b._x >= xMin) && (b._x <= xMax)
                    &&(b._y >= yMin) && (b._y <= yMax)
                      )
                {
                    localline.emplace_back( Line( (Line) iwall ) );
                } else {
                    std::cerr << "GOAL " << goalID << " includes point out of grid!" << std::endl;
                    std::cerr << "Point: " << a._x << ", " << a._y << std::endl;
                    std::cerr << "Point: " << b._x << ", " << b._y << std::endl;
                }
            }

            setNewGoalAfterTheClear(localcostptr, localline);

            //performance-measurement:
            //auto start = std::chrono::steady_clock::now();

            calculateFloorfield(localcostptr, localneggradptr);

            //performance-measurement:
            //auto end = std::chrono::steady_clock::now();
            //auto diff = end - start;
            //std::cerr << std::chrono::duration_cast<std::chrono::seconds>(end - start).count() << std::endl;
            //std::cerr << "new GOALfield " << goalID << "   :    " << localline[0].GetPoint1().GetX() << " " << localline[0].GetPoint1().GetY() << " " << localline[0].GetPoint2().GetX() << " " << localline[0].GetPoint2().GetY() << std::endl;
            //Log->Write("new GOALfield " + std::to_string(goalID) + "  :   " + std::to_string(localline[0].GetPoint1().GetX()));
            //Log->Write("new GOALfield " + std::to_string(goalID) + "  :   " + std::to_string( std::chrono::duration_cast<std::chrono::seconds>(end - start).count() ) + " " + std::to_string(localline.size()) );
            //find closest door and add to cheatmap "goalToLineUID" map

            const std::map<int, Transition*>& transitions = building->GetAllTransitions();
            int UID_of_MIN = -1;
            int UID_of_MIN2 = -1;
            int UID_of_MIN3 = -1;
            double cost_of_MIN = DBL_MAX;
            double cost_of_MIN2 = DBL_MAX;
            double cost_of_MIN3 = DBL_MAX;
            long int dummykey;
            for (const auto& loctrans : transitions) {
                dummykey = grid->getKeyAtPoint(loctrans.second->GetCentre());
                if (cost_of_MIN > localcostptr[dummykey]) {
                    UID_of_MIN3 = UID_of_MIN2;
                    cost_of_MIN3 = cost_of_MIN2;

                    UID_of_MIN2 = UID_of_MIN;
                    cost_of_MIN2 = cost_of_MIN;

                    UID_of_MIN = loctrans.second->GetUniqueID();
                    cost_of_MIN = localcostptr[dummykey];
                    std::cerr << std::endl << "Closer Line found: " << UID_of_MIN ;
                    continue;
                }
                if (cost_of_MIN2 > localcostptr[dummykey]) {
                    UID_of_MIN3 = UID_of_MIN2;
                    cost_of_MIN3 = cost_of_MIN2;

                    UID_of_MIN2 = loctrans.second->GetUniqueID();
                    cost_of_MIN2 = localcostptr[dummykey];
                    continue;
                }
                if (cost_of_MIN3 > localcostptr[dummykey]) {
                    UID_of_MIN3 = loctrans.second->GetUniqueID();
                    cost_of_MIN3 = localcostptr[dummykey];
                    continue;
                }
            }
            goalToLineUIDmap.erase(goalID);
            goalToLineUIDmap.emplace(goalID, UID_of_MIN);
            goalToLineUIDmap2.erase(goalID);
            goalToLineUIDmap2.emplace(goalID, UID_of_MIN2);
            goalToLineUIDmap3.erase(goalID);
            goalToLineUIDmap3.emplace(goalID, UID_of_MIN3);

        }
    }
}

double FloorfieldViaFM::getCostToDestination(const int destID, const Point& position) { //not implemented: trigger calc of new ff not working yet
    if ((costmap.count(destID) == 0) || (costmap.at(destID) == nullptr)) {
        Point dummy;
        getDirectionToUID(destID, 0, dummy);         //this call induces the floorfieldcalculation
    }
    if (costmap.count(destID) == 0) {
        Log->Write("ERROR: \t DestinationUID %d is invalid / out of grid.", destID);
        return DBL_MAX;
    }
    return costmap.at(destID)[grid->getKeyAtPoint(position)];
}

void FloorfieldViaFM::getDir2WallAt(const Point& position, Point& direction){
    long int key = grid->getKeyAtPoint(position);
    direction._x = (dirToWall[key]._x);
    direction._y = (dirToWall[key]._y);
}

double FloorfieldViaFM::getDistance2WallAt(const Point& position) {
    long int key = grid->getKeyAtPoint(position);
    return dist2Wall[key];
}

void FloorfieldViaFM::parseBuilding(const Building* const buildingArg, const double stepSizeX, const double stepSizeY) {
    building = buildingArg;
    //init min/max before parsing
    double xMin = DBL_MAX;
    double xMax = -DBL_MAX;
    double yMin = xMin;
    double yMax = xMax;
    costmap.clear();
    neggradmap.clear();

    //create a list of walls
    const std::map<int, Transition*>& allTransitions = buildingArg->GetAllTransitions();
    for (auto& trans : allTransitions) {
        if (
            trans.second->IsExit() && trans.second->IsOpen()
           )
        {
            wall.emplace_back(Line ( (Line) *(trans.second)));
        }
        //populate both maps: costmap, neggradmap. These are the lookup maps for floorfields to specific transitions
        costmap.emplace(trans.second->GetUniqueID(), nullptr);
        neggradmap.emplace(trans.second->GetUniqueID(), nullptr);
    }
    numOfExits = wall.size();
    for (auto& trans : allTransitions) {
        if (!trans.second->IsOpen()) {
            wall.emplace_back(Line ( (Line) *(trans.second)));
        }

    }
    for (const auto& itRoom : buildingArg->GetAllRooms()) {
        for (const auto& itSubroom : itRoom.second->GetAllSubRooms()) {
            std::vector<Obstacle*> allObstacles = itSubroom.second->GetAllObstacles();
            for (std::vector<Obstacle*>::iterator itObstacles = allObstacles.begin(); itObstacles != allObstacles.end(); ++itObstacles) {

                std::vector<Wall> allObsWalls = (*itObstacles)->GetAllWalls();
                for (std::vector<Wall>::iterator itObsWall = allObsWalls.begin(); itObsWall != allObsWalls.end(); ++itObsWall) {
                    wall.emplace_back(Line( (Line) *itObsWall));
                    // xMin xMax
                    if ((*itObsWall).GetPoint1()._x < xMin) xMin = (*itObsWall).GetPoint1()._x;
                    if ((*itObsWall).GetPoint2()._x < xMin) xMin = (*itObsWall).GetPoint2()._x;
                    if ((*itObsWall).GetPoint1()._x > xMax) xMax = (*itObsWall).GetPoint1()._x;
                    if ((*itObsWall).GetPoint2()._x > xMax) xMax = (*itObsWall).GetPoint2()._x;

                    // yMin yMax
                    if ((*itObsWall).GetPoint1()._y < yMin) yMin = (*itObsWall).GetPoint1()._y;
                    if ((*itObsWall).GetPoint2()._y < yMin) yMin = (*itObsWall).GetPoint2()._y;
                    if ((*itObsWall).GetPoint1()._y > yMax) yMax = (*itObsWall).GetPoint1()._y;
                    if ((*itObsWall).GetPoint2()._y > yMax) yMax = (*itObsWall).GetPoint2()._y;
                }
            }

            std::vector<Wall> allWalls = itSubroom.second->GetAllWalls();
            for (std::vector<Wall>::iterator itWall = allWalls.begin(); itWall != allWalls.end(); ++itWall) {
                wall.emplace_back( Line( (Line) *itWall));

                // xMin xMax
                if ((*itWall).GetPoint1()._x < xMin) xMin = (*itWall).GetPoint1()._x;
                if ((*itWall).GetPoint2()._x < xMin) xMin = (*itWall).GetPoint2()._x;
                if ((*itWall).GetPoint1()._x > xMax) xMax = (*itWall).GetPoint1()._x;
                if ((*itWall).GetPoint2()._x > xMax) xMax = (*itWall).GetPoint2()._x;

                // yMin yMax
                if ((*itWall).GetPoint1()._y < yMin) yMin = (*itWall).GetPoint1()._y;
                if ((*itWall).GetPoint2()._y < yMin) yMin = (*itWall).GetPoint2()._y;
                if ((*itWall).GetPoint1()._y > yMax) yMax = (*itWall).GetPoint1()._y;
                if ((*itWall).GetPoint2()._y > yMax) yMax = (*itWall).GetPoint2()._y;
            }
        }
    }

    //all goals
    const std::map<int, Goal*>& allgoals = buildingArg->GetAllGoals();
    for (auto eachgoal:allgoals) {
        for (auto& eachwall:eachgoal.second->GetAllWalls() ) {
            if (eachwall.GetPoint1()._x < xMin) xMin = eachwall.GetPoint1()._x;
            if (eachwall.GetPoint2()._x < xMin) xMin = eachwall.GetPoint2()._x;
            if (eachwall.GetPoint1()._x > xMax) xMax = eachwall.GetPoint1()._x;
            if (eachwall.GetPoint2()._x > xMax) xMax = eachwall.GetPoint2()._x;

            if (eachwall.GetPoint1()._y < yMin) yMin = eachwall.GetPoint1()._y;
            if (eachwall.GetPoint2()._y < yMin) yMin = eachwall.GetPoint2()._y;
            if (eachwall.GetPoint1()._y > yMax) yMax = eachwall.GetPoint1()._y;
            if (eachwall.GetPoint2()._y > yMax) yMax = eachwall.GetPoint2()._y;
        }
        goalcostmap.emplace(eachgoal.second->GetId(), nullptr);
        goalneggradmap.emplace(eachgoal.second->GetId(), nullptr);
    }

    //create Rect Grid
    grid = new RectGrid();
    grid->setBoundaries(xMin, yMin, xMax, yMax);
    grid->setSpacing(stepSizeX, stepSizeY);
    grid->createGrid();

    //create arrays
    flag = new int[grid->GetnPoints()];                  //flag:( 0 = unknown, 1 = singel, 2 = double, 3 = final, -7 = outside)
    dist2Wall = new double[grid->GetnPoints()];
    speedInitial = new double[grid->GetnPoints()];
    modifiedspeed = new double[grid->GetnPoints()];
    cost = new double[grid->GetnPoints()];
    neggrad = new Point[grid->GetnPoints()];
    dirToWall = new Point[grid->GetnPoints()];
    trialfield = new Trial[grid->GetnPoints()];                 //created with other arrays, but not initialized yet

    costmap.emplace(-1 , cost);                         // enable default ff (closest exit)
    neggradmap.emplace(-1, neggrad);

    //init grid with -3 as unknown distance to any wall
    for(long int i = 0; i < grid->GetnPoints(); ++i) {
        dist2Wall[i] = -3.;
    }
    drawLinesOnGrid(wall, dist2Wall, 0.);
}

//this function must only be used BEFORE calculateDistanceField(), because we set trialfield[].cost = dist2Wall AND we init dist2Wall with "-3"
void FloorfieldViaFM::prepareForDistanceFieldCalculation(std::vector<Line>& lineArg) {
    std::vector<Line> exits(lineArg.begin(), lineArg.begin()+numOfExits);
    drawLinesOnGrid(exits, dist2Wall, -3.); //mark exits as not walls (no malus near exit lines)

    for (long int i = 0; i < grid->GetnPoints(); ++i) {
        if (dist2Wall[i] == 0.) {               //outside or better: wallpoint
            speedInitial[i] = .001;
            cost[i]         = -7.;
            flag[i]         = -7;               //meaning outside
        } else {                                //inside or better: walkable
            speedInitial[i] = 1.;
            cost[i]         = -2.;
            flag[i]         = 0;
        }
        //set Trialptr to fieldelements
        trialfield[i].key = i;
        trialfield[i].flag = flag + i;              //ptr!
        trialfield[i].cost = dist2Wall + i;         //ptr!  //this line imposes, that we calc DistancesField next
        trialfield[i].speed = speedInitial + i;     //ptr!
        trialfield[i].father = nullptr;
        trialfield[i].child = nullptr;
    }
    drawLinesOnGrid(exits, cost, 0.); //already mark targets/exits in cost array (for floorfieldcalc)
    for (long int i=0; i < grid->GetnPoints(); ++i) {
        if (cost[i] == 0.) {            //here we use cost, neggrad directly
            neggrad[i]._x = (0.);        //must be changed to costarray/neggradarray?
            neggrad[i]._y = (0.);        //we can leave it, if we agree on cost/neggrad being
            dirToWall[i]._x = (0.);      //default floorfield using all exits and have the
            dirToWall[i]._y = (0.);      //array mechanic on top
        }
    }
}

void FloorfieldViaFM::clearAndPrepareForFloorfieldReCalc(double* costarray) {
    for (long int i = 0; i < grid->GetnPoints(); ++i) {
        if (dist2Wall[i] == 0.) {    //wall
            costarray[i]    = -7.;                          //this is done in calculateFloorfield again
            flag[i]         = -7;      // meaning wall
        } else {                     //inside
            costarray[i]    = -2.;
            flag[i]         = 0;       // meaning unknown
        }
    }
}

void FloorfieldViaFM::setNewGoalAfterTheClear(double* costarray, std::vector<Line>& LineArg) {
    drawLinesOnGrid(LineArg, costarray, 0.);
    //std::cerr << LineArg[0].GetUniqueID() << " " << LineArg[0].GetPoint1()._x << " " << LineArg[0].GetPoint1()._y << " " << LineArg[0].GetPoint2()._x << " " << LineArg[0].GetPoint2()._y << std::endl;
}

void FloorfieldViaFM::drawLinesOnGrid(std::vector<Line>& wallArg, double* const target, const double outside) { //no init, plz init elsewhere
// i~x; j~y;
//http://stackoverflow.com/questions/10060046/drawing-lines-with-bresenhams-line-algorithm
//src in answer of "Avi"; adapted to fit this application

//    //init with inside value:
//    long int indexMax = grid->GetnPoints();
//    for (long int i = 0; i < indexMax; ++i) {
//        target[i] = inside;
//    }

    //grid handeling local vars:
    long int iMax  = grid->GetiMax();

    long int iStart, iEnd;
    long int jStart, jEnd;
    long int iDot, jDot;
    long int key;
    long int deltaX, deltaY, deltaX1, deltaY1, px, py, xe, ye, i; //Bresenham Algorithm

    for (auto& line : wallArg) {
        key = grid->getKeyAtPoint(line.GetPoint1());
        iStart = grid->get_i_fromKey(key);
        jStart = grid->get_j_fromKey(key);

        key = grid->getKeyAtPoint(line.GetPoint2());
        iEnd = grid->get_i_fromKey(key);
        jEnd = grid->get_j_fromKey(key);

        deltaX = (int) (iEnd - iStart);
        deltaY = (int) (jEnd - jStart);
        deltaX1 = abs( (int) (iEnd - iStart));
        deltaY1 = abs( (int) (jEnd - jStart));

        px = 2*deltaY1 - deltaX1;
        py = 2*deltaX1 - deltaY1;

        if(deltaY1<=deltaX1) {
            if(deltaX>=0) {
                iDot = iStart;
                jDot = jStart;
                xe = iEnd;
            } else {
                iDot = iEnd;
                jDot = jEnd;
                xe = iStart;
            }
            target[jDot*iMax + iDot] = outside;
            for (i=0; iDot < xe; ++i) {
                ++iDot;
                if(px<0) {
                    px+=2*deltaY1;
                } else {
                    if((deltaX<0 && deltaY<0) || (deltaX>0 && deltaY>0)) {
                        ++jDot;
                    } else {
                        --jDot;
                    }
                    px+=2*(deltaY1-deltaX1);
                }
                target[jDot*iMax + iDot] = outside;
            }
        } else {
            if(deltaY>=0) {
                iDot = iStart;
                jDot = jStart;
                ye = jEnd;
            } else {
                iDot = iEnd;
                jDot = jEnd;
                ye = jStart;
            }
            target[jDot*iMax + iDot] = outside;
            for(i=0; jDot<ye; ++i) {
                ++jDot;
                if (py<=0) {
                    py+=2*deltaX1;
                } else {
                    if((deltaX<0 && deltaY<0) || (deltaX>0 && deltaY>0)) {
                        ++iDot;
                    } else {
                        --iDot;
                    }
                    py+=2*(deltaX1-deltaY1);
                }
                target[jDot*iMax + iDot] = outside;
            }
        }
    } //loop over all walls

} //drawLinesOnGrid

void FloorfieldViaFM::setSpeed(bool useDistance2Wall) {
    if (useDistance2Wall && (threshold > 0)) {
        double temp;            //needed to only slowdown band of threshold. outside of band modifiedspeed should be 1
        for (long int i = 0; i < grid->GetnPoints(); ++i) {
            temp = (dist2Wall[i] < threshold) ? dist2Wall[i] : threshold;
            modifiedspeed[i] = 0.001 + 0.999 * (temp/threshold); //linear ramp from wall (0.001) to thresholddistance (1.000)
        }
    } else {
        if (useDistance2Wall) {     //favor middle of hallways/rooms
            for (long int i = 0; i < grid->GetnPoints(); ++i) {
                if (threshold == 0.) {  //special case if user set (use_wall_avoidance = true, wall_avoid_distance = 0.0) and thus wants a plain floorfield
                    modifiedspeed[i] = speedInitial[i];
                } else {                //this is the regular case for "favor middle of hallways/rooms
                    modifiedspeed[i] = 0.001 + 0.999 * (dist2Wall[i]/10); // @todo: ar.graf  (10 ist ein hardgecodeter wert.. sollte ggf. angepasst werden)
                }
            }
        } else {                    //do not use Distance2Wall
            for (long int i = 0; i < grid->GetnPoints(); ++i) {
                modifiedspeed[i] = speedInitial[i];
            }
        }
    }

    //@todo: ar.graf: below is a fix to prevent folks from taking a shortcut outside of rooms. trying to make passing a transition expensive
    std::vector<Line> exits(wall.begin(), wall.begin()+numOfExits);
    drawLinesOnGrid(exits, modifiedspeed, 0.00000000001);

}

void FloorfieldViaFM::calculateFloorfield(double* costarray, Point* neggradarray) {

    Trial* smallest = nullptr;
    Trial* biggest = nullptr;

    //re-init memory
    for (long int i = 0; i < grid->GetnPoints(); ++i) {
        if (dist2Wall[i] == 0.) {               //wall
            flag[i]         = -7;   // -7 => wall
        } else {                                //inside
            flag[i]         = 0;
        }
        //set Trialptr to fieldelements
        trialfield[i].key = i;
        trialfield[i].flag = flag + i;
        trialfield[i].cost = costarray + i;         // @todo: argraf  : setting up trialfield should be separate function, it is done too often for one go
        trialfield[i].neggrad = neggradarray + i;   //                  same holds for init of flag and cost.. watch where to draw (cost=0) line
        trialfield[i].speed = modifiedspeed + i;    //                  it must not be overwritten by any clear procedure
        trialfield[i].father = nullptr;
        trialfield[i].child = nullptr;
    }

    for (long int i = 0; i < grid->GetnPoints(); ++i) {
        if (costarray[i] == 0.) {
            flag[i] = 3;
        }
    }

    //init narrowband
    for (long int i = 0; i < grid->GetnPoints(); ++i) {
        if (flag[i] == 3) {
            checkNeighborsAndAddToNarrowband(smallest, biggest, i, [&] (const long int key) { this->checkNeighborsAndCalcFloorfield(key); } );
        }
    }

    //inital narrowband done, now loop (while not empty: remove smallest, add neighbors of removed)
    while (smallest != nullptr) {
        long int keyOfSmallest = smallest->key;
        flag[keyOfSmallest] = 3;
        trialfield[keyOfSmallest].removecurr(smallest, biggest, trialfield+keyOfSmallest);
        checkNeighborsAndAddToNarrowband(smallest, biggest, keyOfSmallest, [&] (const long int key) { this->checkNeighborsAndCalcFloorfield(key);} );
    }
}

void FloorfieldViaFM::calculateDistanceField(const double thresholdArg) {  //if threshold negative, then ignore it

#ifdef TESTING
    //sanity check (fields <> 0)
    if (flag == 0) return;                  //flag:( 0 = unknown, 1 = singel, 2 = double, 3 = final, 4 = added to trial but not calculated, -7 = outside)
    if (dist2Wall == 0) return;
    if (speedInitial == 0) return;
    if (cost == 0) return;
    if (neggrad == 0) return;
    if (trialfield == 0) return;
#endif //TESTING

    //using dist2Wall-array to store this-function's results, (pseudo)"speedfunction" 1 all around
    //stop if smallest value in narrowband is >= threshold

    //  setting startingpoints of wave (dist2Wall = 0) is done in "parseBuilding"

    //  go thru dist2Wall and add every neighbor of "0"s (only if their flag is 0 and therefore "inside")

    //  HINT: in resetGoalAndCosts, you find: "trialfield[i].cost = dist2Wall + i;"
    //        so here, when we write to "cost", we truely write to "dist2Wall"

    //  HINT: the argument "threshold" is used as a "stop criterion". In the constructor, when calling this,
    //        we pass on thrsholdArg = -1, so we never enter the stop-path.
    Trial* smallest = nullptr;
    Trial* biggest = nullptr;

    for (long int i = 0; i < grid->GetnPoints(); ++i) {
        if (dist2Wall[i] == 0) {
            checkNeighborsAndAddToNarrowband(smallest, biggest, i, [&] (const long int key) { this->checkNeighborsAndCalcDist2Wall(key);} );
        }
    }
    //Log->Write(std::to_string(grid->GetxMax()));
    //Log->Write(std::to_string(grid->GetyMax()));
    //Log->Write(std::to_string(grid->GetxMin()));
    //Log->Write(std::to_string(grid->GetyMin()));
    //Log->Write(std::to_string(grid->GetiMax()));
    //Log->Write(std::to_string(grid->GetjMax()));
    //Log->Write("INFO: \t" + std::to_string(grid->GetnPoints()));
    //inital narrowband done, now loop (while not empty: remove smallest, add neighbors of removed)
    //long int debugcounter = 0;
    while (smallest != nullptr) {
        long int keyOfSmallest = smallest->key;
        flag[keyOfSmallest] = 3;
        if ((thresholdArg > 0) && (trialfield[keyOfSmallest].cost[0] > thresholdArg)) {    //set rest of nearfield and rest of unknown to this max value:

            //rest of nearfield
            Trial* iter = smallest->child;
            while (iter != nullptr) {
                iter[0].cost[0] = trialfield[keyOfSmallest].cost[0];
                iter[0].flag[0] = 3;
                dirToWall[iter[0].key]._x = (0.);
                dirToWall[iter[0].key]._y = (0.);
                iter = iter->child;
            }

            //rest of unknown
            for (long int i = 0; i < grid->GetnPoints(); ++i) {
                if (flag[i] == 0) {
                    flag[i] = 3;
                    dist2Wall[i] = dist2Wall[keyOfSmallest];
                    dirToWall[i]._x = (0.);
                    dirToWall[i]._y = (0.);
                }
            }
            smallest = nullptr;
        } else {
            trialfield[keyOfSmallest].removecurr(smallest, biggest, trialfield+keyOfSmallest);
            //Log->Write(std::to_string(debugcounter++) + " " + std::to_string(grid->GetnPoints()));
            checkNeighborsAndAddToNarrowband(smallest, biggest, keyOfSmallest, [&] (const long int key) { this->checkNeighborsAndCalcDist2Wall(key);} );
        }
    }
} //calculateDistancField

void FloorfieldViaFM::checkNeighborsAndAddToNarrowband(Trial* &smallest, Trial* &biggest, const long int key, std::function<void (const long int)> checkNeighborsAndCalc) {
    long int aux = -1;

    directNeighbor dNeigh = grid->getNeighbors(key);

    //check for valid neigh
    aux = dNeigh.key[0];
    //hint: trialfield[i].cost = dist2Wall + i; <<< set in parseBuilding after linescan call
    //flag:( 0 = unknown, 1 = singel, 2 = double, 3 = final, 4 = added to trial but not calculated, -7 = outside)
    if ((aux != -2) && (flag[aux] == 0)) {
        flag[aux] = 4;      //4 = added to trial but not calculated
        checkNeighborsAndCalc(aux);
        trialfield[aux].insert(smallest, biggest, trialfield + aux);
    }
    aux = dNeigh.key[1];
    if ((aux != -2) && (flag[aux] == 0)) {
        flag[aux] = 4;      //4 = added to trial but not calculated
        checkNeighborsAndCalc(aux);
        trialfield[aux].insert(smallest, biggest, trialfield + aux);
    }
    aux = dNeigh.key[2];
    if ((aux != -2) && (flag[aux] == 0)) {
        flag[aux] = 4;      //4 = added to trial but not calculated
        checkNeighborsAndCalc(aux);
        trialfield[aux].insert(smallest, biggest, trialfield + aux);
    }
    aux = dNeigh.key[3];
    if ((aux != -2) && (flag[aux] == 0)) {
        flag[aux] = 4;      //4 = added to trial but not calculated
        checkNeighborsAndCalc(aux);
        trialfield[aux].insert(smallest, biggest, trialfield + aux);
    }
}

void FloorfieldViaFM::checkNeighborsAndCalcDist2Wall(const long int key) {
    double row;
    double col;
    long int aux;
    bool pointsUp;
    bool pointsRight;

    row = 100000.;
    col = 100000.;
    aux = -1;

    directNeighbor dNeigh = grid->getNeighbors(key);

    aux = dNeigh.key[0];
    //hint: trialfield[i].cost = dist2Wall + i; <<< set in resetGoalAndCosts
    //flag:( 0 = unknown, 1 = singel, 2 = double, 3 = final, 4 = added to trial but not calculated, -7 = outside)
    if  ((aux != -2) &&                                                         //neighbor is a gridpoint
         (flag[aux] != 0))                                                      //gridpoint holds a calculated value
    {
        row = trialfield[aux].cost[0];
        pointsRight = true;
        if (row < 0) {
            std::cerr << "hier ist was schief " << row << " " << aux << " " << flag[aux] << std::endl;
            row = 100000;
        }
    }
    aux = dNeigh.key[2];
    if  ((aux != -2) &&                                                         //neighbor is a gridpoint
         (flag[aux] != 0) &&                                                    //gridpoint holds a calculated value
         (trialfield[aux].cost[0] < row))                                       //calculated value promises smaller cost
    {
        row = trialfield[aux].cost[0];
        pointsRight = false;
    }

    aux = dNeigh.key[1];
    //hint: trialfield[i].cost = dist2Wall + i; <<< set in parseBuilding after linescan call
    //flag:( 0 = unknown, 1 = singel, 2 = double, 3 = final, 4 = added to trial but not calculated, -7 = outside)
    if  ((aux != -2) &&                                                         //neighbor is a gridpoint
         (flag[aux] != 0))                                                      //gridpoint holds a calculated value
    {
        col = trialfield[aux].cost[0];
        pointsUp = true;
        if (col < 0) {
            std::cerr << "hier ist was schief " << col << " " << aux << " " << flag[aux] << std::endl;
            col = 100000;
        }
    }
    aux = dNeigh.key[3];
    if  ((aux != -2) &&                                                         //neighbor is a gridpoint
         (flag[aux] != 0) &&                                                    //gridpoint holds a calculated value
         (trialfield[aux].cost[0] < col))                                       //calculated value promises smaller cost
    {
        col = trialfield[aux].cost[0];
        pointsUp = false;
    }
    if (col == 100000.) { //one sided update with row
        trialfield[key].cost[0] = onesidedCalc(row, grid->Gethx());
        trialfield[key].flag[0] = 1;
        if (pointsRight) {
            dirToWall[key]._x = (-(cost[key+1]-cost[key])/grid->Gethx());
            dirToWall[key]._y = (0.);
        } else {
            dirToWall[key]._x = (-(cost[key]-cost[key-1])/grid->Gethx());
            dirToWall[key]._y = (0.);
        }
        dirToWall[key] = dirToWall[key].Normalized();
        return;
    }

    if (row == 100000.) { //one sided update with col
        trialfield[key].cost[0] = onesidedCalc(col, grid->Gethy());
        trialfield[key].flag[0] = 1;
        if (pointsUp) {
            dirToWall[key]._x = (0.);
            dirToWall[key]._y = (-(cost[key+(grid->GetiMax())]-cost[key])/grid->Gethy());
        } else {
            dirToWall[key]._x = (0.);
            dirToWall[key]._y = (-(cost[key]-cost[key-(grid->GetiMax())])/grid->Gethy());
        }
        dirToWall[key] = dirToWall[key].Normalized();
        return;
    }

    //two sided update
    double precheck = twosidedCalc(row, col, grid->Gethx());
    if (precheck >= 0) {
        trialfield[key].cost[0] = precheck;
        trialfield[key].flag[0] = 2;
        if (pointsUp && pointsRight) {
            dirToWall[key]._x = (-(cost[key+1]-cost[key])/grid->Gethx());
            dirToWall[key]._y = (-(cost[key+(grid->GetiMax())]-cost[key])/grid->Gethy());
        }
        if (pointsUp && !pointsRight) {
            dirToWall[key]._x = (-(cost[key]-cost[key-1])/grid->Gethx());
            dirToWall[key]._y = (-(cost[key+(grid->GetiMax())]-cost[key])/grid->Gethy());
        }
        if (!pointsUp && pointsRight) {
            dirToWall[key]._x = (-(cost[key+1]-cost[key])/grid->Gethx());
            dirToWall[key]._y = (-(cost[key]-cost[key-(grid->GetiMax())])/grid->Gethy());
        }
        if (!pointsUp && !pointsRight) {
            dirToWall[key]._x = (-(cost[key]-cost[key-1])/grid->Gethx());
            dirToWall[key]._y = (-(cost[key]-cost[key-(grid->GetiMax())])/grid->Gethy());
        }
    } else {
        std::cerr << "else in twosided Dist " << std::endl;
    }
    dirToWall[key] = dirToWall[key].Normalized();
}

void FloorfieldViaFM::checkNeighborsAndCalcFloorfield(const long int key) {
    double row;
    double col;
    long int aux;
    bool pointsUp = false;
    bool pointsRight = false;

    row = DBL_MAX;
    col = DBL_MAX;
    aux = -1;


    directNeighbor dNeigh = grid->getNeighbors(key);

    aux = dNeigh.key[0];
    //hint: trialfield[i].cost = costarray + i; <<< set in calculateFloorfield
    //flag:( 0 = unknown, 1 = singel, 2 = double, 3 = final, 4 = added to trial but not calculated, -7 = outside)
    if  ((aux != -2) &&                                                         //neighbor is a gridpoint
         (flag[aux] != 0) &&
         (flag[aux] != -7))                                                      //gridpoint holds a calculated value
    {
        row = trialfield[aux].cost[0];
        pointsRight = true;
    }
    aux = dNeigh.key[2];
    if  ((aux != -2) &&                                                         //neighbor is a gridpoint
         (flag[aux] != 0) &&
         (flag[aux] != -7) &&                                                 //gridpoint holds a calculated value
         (trialfield[aux].cost[0] < row))                                       //calculated value promises smaller cost
    {
        row = trialfield[aux].cost[0];
        pointsRight = false;
    }

    aux = dNeigh.key[1];
    //hint: trialfield[i].cost = cost + i; <<< set in calculateFloorfield
    //flag:( 0 = unknown, 1 = singel, 2 = double, 3 = final, 4 = added to trial but not calculated, -7 = outside)
    if  ((aux != -2) &&                                                         //neighbor is a gridpoint
         (flag[aux] != 0) &&
         (flag[aux] != -7))                                                      //gridpoint holds a calculated value
    {
        col = trialfield[aux].cost[0];
        pointsUp = true;
    }
    aux = dNeigh.key[3];
    if  ((aux != -2) &&                                                         //neighbor is a gridpoint
         (flag[aux] != 0) &&
         (flag[aux] != -7) &&                                                  //gridpoint holds a calculated value
         (trialfield[aux].cost[0] < col))                                       //calculated value promises smaller cost
    {
        col = trialfield[aux].cost[0];
        pointsUp = false;
    }
    if ((col == DBL_MAX) && (row == DBL_MAX)) {
        std::cerr << "Issue 175 in FloorfieldViaFM: invalid combination of row,col (both on max)" <<std::endl;
        return;
    }
    if (col == DBL_MAX) { //one sided update with row
        trialfield[key].cost[0] = onesidedCalc(row, grid->Gethx()/trialfield[key].speed[0]);
        trialfield[key].flag[0] = 1;
        if (pointsRight && (dNeigh.key[0] != -2)) {
            trialfield[key].neggrad[0]._x = (-(trialfield[key+1].cost[0]-trialfield[key].cost[0])/grid->Gethx());
            trialfield[key].neggrad[0]._y = (0.);
        } else if (dNeigh.key[2] != -2) {
            trialfield[key].neggrad[0]._x = (-(trialfield[key].cost[0]-trialfield[key-1].cost[0])/grid->Gethx());
            trialfield[key].neggrad[0]._y = (0.);
        }
        return;
    }

    if (row == DBL_MAX) { //one sided update with col
        trialfield[key].cost[0] = onesidedCalc(col, grid->Gethy()/trialfield[key].speed[0]);
        trialfield[key].flag[0] = 1;
        if ((pointsUp) && (dNeigh.key[1] != -2)) {
            trialfield[key].neggrad[0]._x = (0.);
            trialfield[key].neggrad[0]._y = (-(trialfield[key+(grid->GetiMax())].cost[0]-trialfield[key].cost[0])/grid->Gethy());
        } else if (dNeigh.key[3] != -2){
            trialfield[key].neggrad[0]._x = (0.);
            trialfield[key].neggrad[0]._y = (-(trialfield[key].cost[0]-trialfield[key-(grid->GetiMax())].cost[0])/grid->Gethy());
        }
        return;
    }

    //two sided update
    double precheck = twosidedCalc(row, col, grid->Gethx()/trialfield[key].speed[0]);
    if (precheck >= 0) {
        trialfield[key].cost[0] = precheck;
        trialfield[key].flag[0] = 2;
        if (pointsUp && pointsRight) {
            trialfield[key].neggrad[0]._x = (-(trialfield[key+1].cost[0]-trialfield[key].cost[0])/grid->Gethx());
            trialfield[key].neggrad[0]._y = (-(trialfield[key+(grid->GetiMax())].cost[0]-trialfield[key].cost[0])/grid->Gethy());
        }
        if (pointsUp && !pointsRight) {
            trialfield[key].neggrad[0]._x = (-(trialfield[key].cost[0]-trialfield[key-1].cost[0])/grid->Gethx());
            trialfield[key].neggrad[0]._y = (-(trialfield[key+(grid->GetiMax())].cost[0]-trialfield[key].cost[0])/grid->Gethy());
        }
        if (!pointsUp && pointsRight) {
            trialfield[key].neggrad[0]._x = (-(trialfield[key+1].cost[0]-trialfield[key].cost[0])/grid->Gethx());
            trialfield[key].neggrad[0]._y = (-(trialfield[key].cost[0]-trialfield[key-(grid->GetiMax())].cost[0])/grid->Gethy());
        }
        if (!pointsUp && !pointsRight) {
            trialfield[key].neggrad[0]._x = (-(trialfield[key].cost[0]-trialfield[key-1].cost[0])/grid->Gethx());
            trialfield[key].neggrad[0]._y = (-(trialfield[key].cost[0]-trialfield[key-(grid->GetiMax())].cost[0])/grid->Gethy());
        }
    } else {
        std::cerr << "else in twosided Floor " << precheck << " " << row << " " << col << std::endl;
    }
}

inline double FloorfieldViaFM::onesidedCalc(double xy, double hDivF) {
    //if (xy < 0) std::cerr << "error in onesided " << xy << std::endl;   //todo: performance
    return xy + hDivF;
}

inline double FloorfieldViaFM::twosidedCalc(double x, double y, double hDivF) { //on error return -2
    double determinante = (2*hDivF*hDivF - (x-y)*(x-y));
    if (determinante >= 0) {
        return (x + y + sqrt(determinante))/2;
    } else {
        return (x < y) ? (x + hDivF) : (y + hDivF);
    }
    std::cerr << "error in two-sided 2!!!!!!!!!!!!!!!!!!!!!!! o_O??" << std::endl;
    return -2.; //this line should never execute
} //twosidedCalc

void FloorfieldViaFM::testoutput(const char* filename1, const char* filename2, const double* target) {
//security debug check
    std::ofstream file;
    std::ofstream file2;
    int numX = (int) ((grid->GetxMax()-grid->GetxMin())/grid->Gethx());
    int numY = (int) ((grid->GetyMax()-grid->GetyMin())/grid->Gethy());
    int numTotal = numX * numY;
    //std::cerr << numTotal << " numTotal" << std::endl;
    //std::cerr << grid->GetnPoints() << " grid" << std::endl;
    file.open(filename1);
    file2.open(filename2);
    file << "# vtk DataFile Version 3.0" << std::endl;
    file << "Testdata: Fast Marching: Test: " << std::endl;
    file << "ASCII" << std::endl;
    file << "DATASET STRUCTURED_POINTS" << std::endl;
    file << "DIMENSIONS " <<
                                std::to_string(grid->GetiMax()) <<
                                " " <<
                                std::to_string(grid->GetjMax()) <<
                                " 1" << std::endl;
    file << "ORIGIN " << grid->GetxMin() << " " << grid->GetyMin() << " 0" << std::endl;
    file << "SPACING " << std::to_string(grid->Gethx()) << " " << std::to_string(grid->Gethy()) << " 1" << std::endl;
    file << "POINT_DATA " << std::to_string(numTotal) << std::endl;
    file << "SCALARS Cost float 1" << std::endl;
    file << "LOOKUP_TABLE default" << std::endl;
    for (long int i = 0; i < grid->GetnPoints(); ++i) {
        file << target[i] << std::endl;
        Point iPoint = grid->getPointFromKey(i);
        file2 << iPoint._x /*- grid->GetxMin()*/ << " " << iPoint._y /*- grid->GetyMin()*/ << " " << target[i] << std::endl;
    }

    if (target == cost) {
        file << "VECTORS Gradient float" << std::endl;
        for (int i = 0; i < grid->GetnPoints(); ++i) {
            file << neggrad[i]._x << " " << neggrad[i]._y << " 0.0" << std::endl;
        }
    }

    file.close();
    file2.close();

    //std::cerr << "INFO: \tFile closed: " << filename1 << std::endl;
}

void FloorfieldViaFM::writeFF(const std::string& filename) {
    Log->Write("INFO: \tWrite Floorfield to file <" +  filename + ">");
    std::ofstream file;

    int numX = (int) ((grid->GetxMax()-grid->GetxMin())/grid->Gethx());
    int numY = (int) ((grid->GetyMax()-grid->GetyMin())/grid->Gethy());
    int numTotal = numX * numY;
    //std::cerr << numTotal << " numTotal" << std::endl;
    //std::cerr << grid->GetnPoints() << " grid" << std::endl;
    file.open(filename);

    file << "# vtk DataFile Version 3.0" << std::endl;
    file << "Testdata: Fast Marching: Test: " << std::endl;
    file << "ASCII" << std::endl;
    file << "DATASET STRUCTURED_POINTS" << std::endl;
    file << "DIMENSIONS " <<
                                std::to_string(grid->GetiMax()) <<
                                " " <<
                                std::to_string(grid->GetjMax()) <<
                                " 1" << std::endl;
    file << "ORIGIN " << grid->GetxMin() << " " << grid->GetyMin() << " 0" << std::endl;
    file << "SPACING " << std::to_string(grid->Gethx()) << " " << std::to_string(grid->Gethy()) << " 1" << std::endl;
    file << "POINT_DATA " << std::to_string(numTotal) << std::endl;
    file << "SCALARS Dist2Wall float 1" << std::endl;
    file << "LOOKUP_TABLE default" << std::endl;
    for (long int i = 0; i < grid->GetnPoints(); ++i) {
        file << dist2Wall[i] << std::endl; //@todo: change target to all dist2wall
        //Point iPoint = grid->getPointFromKey(i);
        //file2 << iPoint._x /*- grid->GetxMin()*/ << " " << iPoint._y /*- grid->GetyMin()*/ << " " << target[i] << std::endl;
    }

    file << "VECTORS Gradient float" << std::endl;
    for (int i = 0; i < grid->GetnPoints(); ++i) {
        file << neggrad[i]._x << " " << neggrad[i]._y << " 0.0" << std::endl;
    }

    file << "VECTORS Dir2Wall float" << std::endl;
    for (int i = 0; i < grid->GetnPoints(); ++i) {
        file << dirToWall[i]._x << " " << dirToWall[i]._y << " 0.0" << std::endl;
    }

    if (cost != nullptr) {
        file << "SCALARS Cost float 1" << std::endl;
        file << "LOOKUP_TABLE default" << std::endl;
        for (long int i = 0; i < grid->GetnPoints(); ++i) {
            file << cost[i] << std::endl;
        }
    }
    file.close();
}
