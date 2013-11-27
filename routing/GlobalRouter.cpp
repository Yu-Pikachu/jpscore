/**
 * @file    GlobalRouter.cpp
 * @author  Ulrich Kemloh <kemlohulrich@gmail.com>
 * Created on: Dec 15, 2010
 * Copyright (C) <2009-2011>
 *
 * @section LICENSE
 * This file is part of JuPedSim.
 *
 * JuPedSim is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * JuPedSim is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with JuPedSim. If not, see <http://www.gnu.org/licenses/>.
 *
 * @section DESCRIPTION
 *
 *
 *
 */

#include "GlobalRouter.h"

#include "AccessPoint.h"
#include "Router.h"
#include "../geometry/Building.h"
#include "../pedestrian/Pedestrian.h"
#include "../tinyxml/tinyxml.h"

#include <sstream>
#include <cfloat>
#include <fstream>
#include <iomanip>


using namespace std;

GlobalRouter::GlobalRouter() :
				Router() {
	_accessPoints = map<int, AccessPoint*>();
	_map_id_to_index = std::map<int, int>();
	_map_index_to_id = std::map<int, int>();
	_distMatrix = NULL;
	_pathsMatrix = NULL;
	_building = NULL;

}

GlobalRouter::~GlobalRouter() {

	if (_distMatrix && _pathsMatrix) {
		const int exitsCnt = _building->GetNumberOfGoals();
		for (int p = 0; p < exitsCnt; ++p) {
			delete[] _distMatrix[p];
			delete[] _pathsMatrix[p];
		}

		delete[] _distMatrix;
		delete[] _pathsMatrix;
	}

	map<int, AccessPoint*>::const_iterator itr;
	for (itr = _accessPoints.begin(); itr != _accessPoints.end(); ++itr) {
		delete itr->second;
	}
	_accessPoints.clear();
}

void GlobalRouter::Init(Building* building) {

	Log->Write("INFO:\tInit the Global Router Engine");
	_building = building;
	LoadRoutingInfos(GetRoutingInfoFile());

	// initialize the network for the floyd warshall algo
	// initialize the distances matrix

	const int exitsCnt = _building->GetNumberOfGoals() + _building->GetAllGoals().size();

	_distMatrix = new double*[exitsCnt];
	_pathsMatrix = new int*[exitsCnt];

	for (int i = 0; i < exitsCnt; ++i) {
		_distMatrix[i] = new double[exitsCnt];
		_pathsMatrix[i] = new int[exitsCnt];
	}
	//	initializing the values
	// all nodes are disconnected
	for (int p = 0; p < exitsCnt; ++p){
		for (int r = 0; r < exitsCnt; ++r) {
			_distMatrix[p][r] = (r == p) ? 0.0 : FLT_MAX;/*0.0*/
			_pathsMatrix[p][r] = p;/*0.0*/
		}
	}

	// init the access points
	int index = 0;

	for (map<int, Hline*>::const_iterator itr = _building->GetAllHlines().begin();
			itr != _building->GetAllHlines().end(); ++itr) {

		//int door=itr->first;
		int door = itr->second->GetUniqueID();
		Hline* cross = itr->second;
		Point centre = cross->GetCentre();
		double center[2] = { centre.GetX(), centre.GetY() };

		AccessPoint* ap = new AccessPoint(door, center);
		ap->SetNavLine(cross);
		char friendlyName[CLENGTH];
		sprintf(friendlyName, "hline_%d_room_%d_subroom_%d", cross->GetID(),
				cross->GetRoom()->GetID(),
				cross->GetSubRoom()->GetSubRoomID());
		ap->SetFriendlyName(friendlyName);

		// save the connecting sub/rooms IDs
		int id1 = -1;
		if (cross->GetSubRoom()) {
			id1 = cross->GetSubRoom()->GetUID();
		}

		ap->setConnectingRooms(id1, id1);
		_accessPoints[door] = ap;

		//very nasty
		_map_id_to_index[door] = index;
		_map_index_to_id[index] = door;
		index++;
	}


	for (map<int, Crossing*>::const_iterator itr = _building->GetAllCrossings().begin();
			itr != _building->GetAllCrossings().end(); ++itr) {

		int door = itr->second->GetUniqueID();
		Crossing* cross = itr->second;
		const Point& centre = cross->GetCentre();
		double center[2] = { centre.GetX(), centre.GetY() };

		AccessPoint* ap = new AccessPoint(door, center);
		ap->SetNavLine(cross);
		char friendlyName[CLENGTH];
		sprintf(friendlyName, "cross_%d_room_%d_subroom_%d", cross->GetID(),
				cross->GetRoom1()->GetID(),
				cross->GetSubRoom1()->GetSubRoomID());
		ap->SetFriendlyName(friendlyName);

		// save the connecting sub/rooms IDs
		int id1 = -1;
		if (cross->GetSubRoom1()) {
			id1 = cross->GetSubRoom1()->GetUID();
		}

		int id2 = -1;
		if (cross->GetSubRoom2()) {
			id2 = cross->GetSubRoom2()->GetUID();
		}

		ap->setConnectingRooms(id1, id2);
		_accessPoints[door] = ap;

		//very nasty
		_map_id_to_index[door] = index;
		_map_index_to_id[index] = door;
		index++;
	}

	for (map<int, Transition*>::const_iterator itr = _building->GetAllTransitions().begin();
			itr != _building->GetAllTransitions().end(); ++itr) {

		int door = itr->second->GetUniqueID();
		Transition* cross = itr->second;
		const Point& centre = cross->GetCentre();
		double center[2] = { centre.GetX(), centre.GetY() };

		AccessPoint* ap = new AccessPoint(door, center);
		ap->SetNavLine(cross);
		char friendlyName[CLENGTH];
		sprintf(friendlyName, "trans_%d_room_%d_subroom_%d", cross->GetID(),
				cross->GetRoom1()->GetID(),
				cross->GetSubRoom1()->GetSubRoomID());
		ap->SetFriendlyName(friendlyName);

		ap->SetClosed(!cross->IsOpen());
		// save the connecting sub/rooms IDs
		int id1 = -1;
		if (cross->GetSubRoom1()) {
			id1 = cross->GetSubRoom1()->GetUID();
		}

		int id2 = -1;
		if (cross->GetSubRoom2()) {
			id2 = cross->GetSubRoom2()->GetUID();
		}

		ap->setConnectingRooms(id1, id2);
		_accessPoints[door] = ap;

		//set the final destination
		if (cross->IsExit() && cross->IsOpen()) {
			ap->SetFinalExitToOutside(true);
			Log->Write("INFO: \tExit to outside found: %d [%s]",ap->GetID(),ap->GetFriendlyName().c_str());
		} else if ((id1 == -1) && (id2 == -1)) {
			Log->Write(" a final destination outside the geometry was found");
			ap->SetFinalExitToOutside(true);
		} else if (cross->GetRoom1()->GetCaption() == "outside") {
			ap->SetFinalExitToOutside(true);
		}

		//very nasty
		_map_id_to_index[door] = index;
		_map_index_to_id[index] = door;
		index++;

	}

	// loop over the rooms
	// loop over the subrooms
	// get the transitions in the subrooms
	// and compute the distances

	for (int i = 0; i < _building->GetNumberOfRooms(); i++) {
		Room* room = _building->GetRoom(i);

		for (int j = 0; j < room->GetNumberOfSubRooms(); j++) {

			SubRoom* sub = room->GetSubRoom(j);

			//collect all navigation objects
			vector<NavLine*> allGoals;
			const vector<Crossing*>& crossings = sub->GetAllCrossings();
			allGoals.insert(allGoals.end(), crossings.begin(), crossings.end());
			const vector<Transition*>& transitions = sub->GetAllTransitions();
			allGoals.insert(allGoals.end(), transitions.begin(),
					transitions.end());
			const vector<Hline*>& hlines = sub->GetAllHlines();
			allGoals.insert(allGoals.end(), hlines.begin(), hlines.end());

			//process the hlines
			//process the crossings
			//process the transitions
			for (unsigned int n1 = 0; n1 < allGoals.size(); n1++) {

				NavLine* nav1 = allGoals[n1];
				AccessPoint* from_AP = _accessPoints[nav1->GetUniqueID()];
				int from_door = _map_id_to_index[nav1->GetUniqueID()];

				for (unsigned int n2 = 0; n2 < allGoals.size(); n2++) {
					NavLine* nav2 = allGoals[n2];

					if (n1 == n2)
						continue;
					if (nav1->operator ==(*nav2))
						continue;

					if (sub->IsVisible(nav1, nav2, true)) {
						int to_door = _map_id_to_index[nav2->GetUniqueID()];
						_distMatrix[from_door][to_door] = (nav1->GetCentre()
								- nav2->GetCentre()).Norm();
						from_AP->AddConnectingAP(
								_accessPoints[nav2->GetUniqueID()]);
					}
				}
			}
		}
	}

	for (unsigned int p = 0; p < _finalDestinations.size(); p++) {

		Goal* goal =_building->GetFinalGoal(_finalDestinations[p]);
		const Wall& line=_building->GetFinalGoal(_finalDestinations[p])->GetAllWalls()[0];
		double center[2] = { goal->GetCentroid()._x, goal->GetCentroid()._y };


		AccessPoint* to_AP = new AccessPoint(line.GetUniqueID(), center);
		to_AP->SetFinalGoalOutside(true);
		to_AP->SetNavLine(new NavLine(line));
		char friendlyName[CLENGTH];
		sprintf(friendlyName, "finalGoal_%d_located_outside", goal->GetId());
		to_AP->SetFriendlyName(friendlyName);
		to_AP->AddFinalDestination(FINAL_DEST_OUT,0.0);
		to_AP->AddFinalDestination(goal->GetId(),0.0);
		_accessPoints[to_AP->GetID()] = to_AP;

		//very nasty
		_map_id_to_index[to_AP->GetID()] = index;
		_map_index_to_id[index] = to_AP->GetID();
		index++;

		//only make a connection to final exit to outside
		for (map<int, AccessPoint*>::const_iterator itr1 =
				_accessPoints.begin(); itr1 != _accessPoints.end(); ++itr1) {
			AccessPoint* from_AP = itr1->second;
			if(from_AP->GetFinalExitToOutside()==false) continue;
			if(from_AP->GetID()==to_AP->GetID()) continue;
			from_AP->AddConnectingAP(to_AP);
			int from_door= _map_id_to_index[from_AP->GetID()];
			int to_door= _map_id_to_index[to_AP->GetID()];
			_distMatrix[from_door][to_door] = from_AP->GetNavLine()->DistTo(goal->GetCentroid());
		}
	}

	//run the floyd warshall algorithm
	FloydWarshall();


	// set the configuration for reaching the outside
	// set the distances to all final APs

	for (map<int, AccessPoint*>::const_iterator itr = _accessPoints.begin();
			itr != _accessPoints.end(); ++itr) {

		AccessPoint* from_AP = itr->second;
		int from_door = _map_id_to_index[itr->first];
		if(from_AP->GetFinalGoalOutside()) continue;

		double tmpMinDist = FLT_MAX;
		int tmpFinalGlobalNearestID = from_door;

		for (map<int, AccessPoint*>::const_iterator itr1 =
				_accessPoints.begin(); itr1 != _accessPoints.end(); ++itr1) {

			AccessPoint* to_AP = itr1->second;

			if(from_AP->GetID()==to_AP->GetID()) continue;
			//if(from_AP->GetFinalExitToOutside()) continue;
			//if(from_AP->GetFinalGoalOutside()) continue;

			if (to_AP->GetFinalExitToOutside()) {

				int to_door = _map_id_to_index[itr1->first];
				if (from_door == to_door)
					continue;

				//cout <<" checking final destination: "<< pAccessPoints[j]->GetID()<<endl;
				double dist = _distMatrix[from_door][to_door];
				if (dist < tmpMinDist) {
					tmpFinalGlobalNearestID = to_door;
					tmpMinDist = dist;
				}
			}
		}

		// in the case it is the final APs
		if (tmpFinalGlobalNearestID == from_door)
			tmpMinDist = 0.0;

		if (tmpMinDist == FLT_MAX) {
			Log->Write(
					"ERROR: GlobalRouter: There is no visibility path from [%s] to the outside 1\n",
					from_AP->GetFriendlyName().c_str());
			from_AP->Dump();
			exit(EXIT_FAILURE);
		}

		// set the distance to the final destination ( OUT )
		from_AP->AddFinalDestination(FINAL_DEST_OUT, tmpMinDist);

		// set the intermediate path to global final destination
		GetPath(from_door, tmpFinalGlobalNearestID);

		if (_tmpPedPath.size() >= 2) {
			from_AP->AddTransitAPsTo(FINAL_DEST_OUT,
					_accessPoints[_map_index_to_id[_tmpPedPath[1]]]);
		} else {
			if ((!from_AP->GetFinalExitToOutside())
					&& (!from_AP->IsClosed())) {

				Log->Write(
						"ERROR: GlobalRouter: There is no visibility path from [%s] to the outside 2\n",
						from_AP->GetFriendlyName().c_str());
				from_AP->Dump();
				exit(EXIT_FAILURE);
			}
		}
		_tmpPedPath.clear();
	}


	// set the configuration to reach the goals specified in the ini file
	// set the distances to alternative destinations

	for (unsigned int p = 0; p < _finalDestinations.size(); p++) {

		int to_door_uid =
				_building->GetFinalGoal(_finalDestinations[p])->GetAllWalls()[0].GetUniqueID();
		int to_door_matrix_index=_map_id_to_index[to_door_uid];

		// thats probably a goal located outside the geometry or not an exit from the geometry
		if(to_door_uid==-1){
			Log->Write(
					"ERROR: \tGlobalRouter: there is something wrong with final destination [ %d ]\n",
					_finalDestinations[p]);
			exit(EXIT_FAILURE);
		}

		for (map<int, AccessPoint*>::const_iterator itr =
				_accessPoints.begin(); itr != _accessPoints.end(); ++itr) {

			AccessPoint* from_AP = itr->second;
			if(from_AP->GetFinalGoalOutside()) continue;
			int from_door_matrix_index = _map_id_to_index[itr->first];

			//comment this if you want infinite as distance to unreachable destinations
			double dist = _distMatrix[from_door_matrix_index][to_door_matrix_index];
			from_AP->AddFinalDestination(_finalDestinations[p], dist);

			// set the intermediate path
			// set the intermediate path to global final destination
			GetPath(from_door_matrix_index, to_door_matrix_index);
			if (_tmpPedPath.size() >= 2) {
				from_AP->AddTransitAPsTo(_finalDestinations[p],
						_accessPoints[_map_index_to_id[_tmpPedPath[1]]]);
			} else {
				if (((!from_AP->IsClosed()))) {
					Log->Write(
							"ERROR: GlobalRouter: There is no visibility path from [%s] to goal [%d]\n",
							from_AP->GetFriendlyName().c_str(), _finalDestinations[p]);
					from_AP->Dump();
					exit(EXIT_FAILURE);
				}
			}
			_tmpPedPath.clear();
		}
	}

	//dumping the complete system
	//DumpAccessPoints(19);
	//vector<string> rooms;
	//rooms.push_back("hall");
	//rooms.push_back("0");
	//rooms.push_back("1");
	//rooms.push_back("2");
	//WriteGraphGV("routing_graph.gv",FINAL_DEST_OUT,rooms);
	//WriteGraphGV("routing_graph.gv",1,rooms);
	Log->Write("INFO:\tDone with the Global Router Engine!");
	//exit(0);
}

void GlobalRouter::GetPath(int i, int j) {
	if (_distMatrix[i][j] == FLT_MAX)
		return;
	if (i != j)
		GetPath(i, _pathsMatrix[i][j]);
	_tmpPedPath.push_back(j);
	//printf("--%d--",j);
}

/*
 floyd_warshall()

 after calling this function dist[i][j] will the the minimum distance
 between i and j if it exists (i.e. if there's a path between i and j)
 or 0, otherwise
 */
void GlobalRouter::FloydWarshall() {
	//	int i, j, k;
	const int n = _building->GetNumberOfGoals() + _building->GetAllGoals().size();;

	for (int k = 0; k < n; k++)
		for (int i = 0; i < n; i++)
			for (int j = 0; j < n; j++)
				if (_distMatrix[i][k] + _distMatrix[k][j] < _distMatrix[i][j]) {
					_distMatrix[i][j] = _distMatrix[i][k] + _distMatrix[k][j];
					_pathsMatrix[i][j] = _pathsMatrix[k][j];
				}
	return;

}

void GlobalRouter::DumpAccessPoints(int p) {

	if (p != -1) {
		_accessPoints.at(p)->Dump();
	} else {
		for (map<int, AccessPoint*>::const_iterator itr = _accessPoints.begin();
				itr != _accessPoints.end(); ++itr) {
			itr->second->Dump();
		}
	}
}

int GlobalRouter::FindExit(Pedestrian* ped) {

	int nextDestination = ped->GetNextDestination();
	//ped->Dump(1);

	if (nextDestination == -1) {
		return GetBestDefaultRandomExit(ped);

	} else {

		SubRoom* sub = _building->GetRoom(ped->GetRoomID())->GetSubRoom(
				ped->GetSubRoomID());

		const vector<int>& accessPointsInSubRoom = sub->GetAllGoalIDs();
		for (unsigned int i = 0; i < accessPointsInSubRoom.size(); i++) {

			int apID = accessPointsInSubRoom[i];
			AccessPoint* ap = _accessPoints[apID];

			const Point& pt3 = ped->GetPos();
			double distToExit = ap->GetNavLine()->DistTo(pt3);

			if (distToExit > J_EPS_DIST)
				continue;

			//one AP is near actualize destination:
			nextDestination = ap->GetNearestTransitAPTO(
					ped->GetFinalDestination());


			if (nextDestination == -1) { // we are almost at the exit
				return ped->GetNextDestination();
			} else {
				//check that the next destination is in the actual room of the pedestrian
				if (_accessPoints[nextDestination]->isInRange(
						sub->GetUID())==false) {
					//return the last destination if defined
					int previousDestination = ped->GetNextDestination();

					//we are still somewhere in the initialization phase
					if (previousDestination == -1) {
						ped->SetExitIndex(apID);
						ped->SetExitLine(_accessPoints[apID]->GetNavLine());
						return apID;
					} else // we are still having a valid destination, don't change
					{
						return previousDestination;
					}
				} else // we have reached the new room
				{
					ped->SetExitIndex(nextDestination);
					ped->SetExitLine(
							_accessPoints[nextDestination]->GetNavLine());
					return nextDestination;
				}
			}
		}

		// still have a valid destination, so return it
		return nextDestination;
	}
}

int GlobalRouter::GetBestDefaultRandomExit(Pedestrian* ped) {
	// get the opened exits
	SubRoom* sub = _building->GetRoom(ped->GetRoomID())->GetSubRoom(
			ped->GetSubRoomID());

	// get the opened exits
	const vector<int>& accessPointsInSubRoom = sub->GetAllGoalIDs();
	int bestAPsID = -1;
	double minDist = FLT_MAX;

	for (unsigned int i = 0; i < accessPointsInSubRoom.size(); i++) {
		int apID = accessPointsInSubRoom[i];

		AccessPoint* ap = _accessPoints[apID];

		if (ap->isInRange(sub->GetUID()) == false)
			continue;

		//check if that exit is open.
		if (ap->IsClosed())
			continue;

		//the line from the current position to the centre of the nav line.
		// at least the line in that direction minus EPS
		const Point& posA = ped->GetPos();
		const Point& posB = ap->GetNavLine()->GetCentre();
		const Point& posC = (posB - posA).Normalized()
				* ((posA - posB).Norm() - J_EPS) + posA;

		//check if visible
		if (sub->IsVisible(posA, posC, true) == false)
			continue;

		double dist = ap->GetDistanceTo(ped->GetFinalDestination())
				+ ap->DistanceTo(posA.GetX(), posA.GetY());

		if (dist < minDist) {
			bestAPsID = ap->GetID();
			minDist = dist;
		}
	}

	if (bestAPsID != -1) {
		ped->SetExitIndex(bestAPsID);
		ped->SetExitLine(_accessPoints[bestAPsID]->GetNavLine());
		return bestAPsID;
	} else {
		if (_building->GetRoom(ped->GetRoomID())->GetCaption() != "outside")
			Log->Write(
					"ERROR:\t Cannot find valid destination for ped [%d] located in room [%d] subroom [%d] going to destination [%d]",
					ped->GetID(), ped->GetRoomID(), ped->GetSubRoomID(),
					ped->GetFinalDestination());
		//exit(EXIT_FAILURE);
		return -1;
	}
}


bool GlobalRouter::CanSeeEachOther(Crossing* c1, Crossing* c2) {

	//do they share at least one subroom?
	//find the common subroom,
	//return false if none
	SubRoom* sb1_a = c1->GetSubRoom1();
	SubRoom* sb1_b = c1->GetSubRoom2();

	SubRoom* sb2_a = c2->GetSubRoom1();
	SubRoom* sb2_b = c2->GetSubRoom2();

	SubRoom* sub = NULL;

	if ((sb1_a != NULL) && (sb1_a == sb2_a))
		sub = sb1_a;
	else if ((sb1_a != NULL) && (sb1_a == sb2_b))
		sub = sb1_a;
	else if ((sb1_b != NULL) && (sb1_b == sb2_a))
		sub = sb1_b;
	else if ((sb1_b != NULL) && (sb1_b == sb2_b))
		sub = sb1_b;

	if (sub == NULL) {
		//char tmp[CLENGTH];
		//sprintf(tmp,"no common subroom found for transitions [%d] and [%d]",
		//		c1->GetIndex(),c2->GetIndex());
		//Log->write(tmp);
		return false;
	}

	// segment connecting the two APs/goals
	const Point& p1 = (c1->GetPoint1() + c1->GetPoint2()) * 0.5;
	const Point& p2 = (c2->GetPoint1() + c2->GetPoint2()) * 0.5;
	Line segment = Line(p1, p2);

	// check if this in intersected by any other connections/walls/doors/trans/cross in the room

	//first walls
	const vector<Wall>& walls = sub->GetAllWalls();

	for (unsigned int b = 0; b < walls.size(); b++) {
		if (segment.IntersectionWith(walls[b]) == true) {
			return false;
		}
	}

	// also take into account other crossings/transitions
	const vector<int>& exitsInSubroom = sub->GetAllGoalIDs();

	int id1 = c1->GetID();
	int id2 = c2->GetID();
	// then all goals
	for (int g = 0; g < (int) exitsInSubroom.size(); g++) {
		int gID = exitsInSubroom[g];
		// skip the concerned exits door and d
		if ((id1 == gID) || (id2 == gID))
			continue;
		if (segment.IntersectionWith(*_building->GetGoal(exitsInSubroom[g])) == true) {
			return false;
		}
	}

	//last check in the case of a concav polygon
	// check if the middle of the connection line lies inside the subroom
	Point middle = (p1 + p2) * 0.5;
	bool isVisible = sub->IsInSubRoom(middle);

	if (isVisible == false) {
		return false;
	}

	return true;
}

SubRoom* GlobalRouter::GetCommonSubRoom(Crossing* c1, Crossing* c2) {
	SubRoom* sb11 = c1->GetSubRoom1();
	SubRoom* sb12 = c1->GetSubRoom2();
	SubRoom* sb21 = c2->GetSubRoom1();
	SubRoom* sb22 = c2->GetSubRoom2();

	if (sb11 == sb21)
		return sb11;
	if (sb11 == sb22)
		return sb11;
	if (sb12 == sb21)
		return sb12;
	if (sb12 == sb22)
		return sb12;

	return NULL;
}

void GlobalRouter::WriteGraphGV(string filename, int finalDestination,
		const vector<string> rooms_captions) {
	ofstream graph_file(filename.c_str());
	if (graph_file.is_open() == false) {
		Log->Write("Unable to open file" + filename);
		return;
	}

	//header
	graph_file << "## Produced by OPS_GCFM" << endl;
	//graph_file << "##comand: \" sfdp -Goverlap=prism -Gcharset=latin1"<<filename <<"| gvmap -e | neato -Ecolor=\"#55555522\" -n2 -Tpng > "<< filename<<".png \""<<endl;
	graph_file << "##Command to produce the output: \"neato -n -s -Tpng "
			<< filename << " > " << filename << ".png\"" << endl;
	graph_file << "digraph OPS_GCFM_ROUTING {" << endl;
	graph_file << "overlap=scale;" << endl;
	graph_file << "splines=false;" << endl;
	graph_file << "fontsize=20;" << endl;
	graph_file
	<< "label=\"Graph generated by the routing engine for destination: "
	<< finalDestination << "\"" << endl;

	vector<int> rooms_ids = vector<int>();

	if (rooms_captions.empty()) {
		// then all rooms should be printed
		for (int i = 0; i < _building->GetNumberOfRooms(); i++) {
			rooms_ids.push_back(i);
		}

	} else {
		for (unsigned int i = 0; i < rooms_captions.size(); i++) {
			rooms_ids.push_back(
					_building->GetRoom(rooms_captions[i])->GetID());
		}
	}

	for (map<int, AccessPoint*>::const_iterator itr = _accessPoints.begin();
			itr != _accessPoints.end(); ++itr) {

		AccessPoint* from_AP = itr->second;

		int from_door = from_AP->GetID();

		// check for valid room
		NavLine* nav = from_AP->GetNavLine();
		int room_id = -1;

		if (dynamic_cast<Crossing*>(nav) != NULL) {
			room_id = ((Crossing*) (nav))->GetRoom1()->GetID();

		} else if (dynamic_cast<Hline*>(nav) != NULL) {
			room_id = ((Hline*) (nav))->GetRoom()->GetID();

		} else if (dynamic_cast<Transition*>(nav) != NULL) {
			room_id = ((Transition*) (nav))->GetRoom1()->GetID();

		} else {
			cout << "WARNING: Unkown navigation line type" << endl;
			continue;
		}

		if (IsElementInVector(rooms_ids, room_id) == false)
			continue;

		double px = from_AP->GetCentre().GetX();
		double py = from_AP->GetCentre().GetY();
		//graph_file << from_door <<" [shape=ellipse, pos=\""<<px<<", "<<py<<" \"] ;"<<endl;
		//graph_file << from_door <<" [shape=ellipse, pos=\""<<px<<","<<py<<"\" ];"<<endl;

		//const vector<AccessPoint*>& from_aps = from_AP->GetConnectingAPs();
		const vector<AccessPoint*>& from_aps = from_AP->GetTransitAPsTo(
				finalDestination);

		if (from_aps.size() == 0) {

			if (from_AP->GetFinalExitToOutside()) {
				graph_file << from_door << " [pos=\"" << px << ", " << py
						<< " \", style=filled, color=green,fontsize=5] ;"
						<< endl;
				//				graph_file << from_door <<" [width=\"0.41\", height=\"0.31\",fixedsize=false,pos=\""<<px<<", "<<py<<" \", style=filled, color=green,fontsize=4] ;"<<endl;
			} else {
				graph_file << from_door << " [pos=\"" << px << ", " << py
						<< " \", style=filled, color=red,fontsize=5] ;" << endl;
				//				graph_file << from_door <<" [width=\"0.41\", height=\"0.31\",fixedsize=false,pos=\""<<px<<", "<<py<<" \", style=filled, color=red,fontsize=4] ;"<<endl;
			}
		} else {
			// check that all connecting aps are contained in the room_ids list
			// if not marked as sink.
			bool isSink = true;
			for (unsigned int j = 0; j < from_aps.size(); j++) {
				NavLine* nav = from_aps[j]->GetNavLine();
				int room_id = -1;

				if (dynamic_cast<Crossing*>(nav) != NULL) {
					room_id = ((Crossing*) (nav))->GetRoom1()->GetID();

				} else if (dynamic_cast<Hline*>(nav) != NULL) {
					room_id = ((Hline*) (nav))->GetRoom()->GetID();

				} else if (dynamic_cast<Transition*>(nav) != NULL) {
					room_id = ((Transition*) (nav))->GetRoom1()->GetID();

				} else {
					cout << "WARNING: Unkown navigation line type" << endl;
					continue;
				}

				if (IsElementInVector(rooms_ids, room_id) == true) {
					isSink = false;
					break;
				}
			}

			if (isSink) {
				//graph_file << from_door <<" [width=\"0.3\", height=\"0.21\",fixedsize=false,pos=\""<<px<<", "<<py<<" \" ,style=filled, color=green, fontsize=4] ;"<<endl;
				graph_file << from_door << " [pos=\"" << px << ", " << py
						<< " \" ,style=filled, color=blue, fontsize=5] ;"
						<< endl;
			} else {
				//graph_file << from_door <<" [width=\"0.3\", height=\"0.231\",fixedsize=false, pos=\""<<px<<", "<<py<<" \", fontsize=4] ;"<<endl;
				graph_file << from_door << " [pos=\"" << px << ", " << py
						<< " \", style=filled, color=yellow, fontsize=5] ;"
						<< endl;
			}
		}

	}

	//connections

	for (map<int, AccessPoint*>::const_iterator itr = _accessPoints.begin();
			itr != _accessPoints.end(); ++itr) {

		AccessPoint* from_AP = itr->second;
		int from_door = from_AP->GetID();

		//const vector<AccessPoint*>& aps = from_AP->GetConnectingAPs();
		const vector<AccessPoint*>& aps = from_AP->GetTransitAPsTo(
				finalDestination);

		NavLine* nav = from_AP->GetNavLine();
		int room_id = -1;

		if (dynamic_cast<Crossing*>(nav) != NULL) {
			room_id = ((Crossing*) (nav))->GetRoom1()->GetID();

		} else if (dynamic_cast<Hline*>(nav) != NULL) {
			room_id = ((Hline*) (nav))->GetRoom()->GetID();

		} else if (dynamic_cast<Transition*>(nav) != NULL) {
			room_id = ((Transition*) (nav))->GetRoom1()->GetID();

		} else {
			cout << "WARNING: Unkown navigation line type" << endl;
			continue;
		}

		if (IsElementInVector(rooms_ids, room_id) == false)
			continue;

		for (unsigned int j = 0; j < aps.size(); j++) {
			AccessPoint* to_AP = aps[j];
			int to_door = to_AP->GetID();

			NavLine* nav = to_AP->GetNavLine();
			int room_id = -1;

			if (dynamic_cast<Crossing*>(nav) != NULL) {
				room_id = ((Crossing*) (nav))->GetRoom1()->GetID();

			} else if (dynamic_cast<Hline*>(nav) != NULL) {
				room_id = ((Hline*) (nav))->GetRoom()->GetID();

			} else if (dynamic_cast<Transition*>(nav) != NULL) {
				room_id = ((Transition*) (nav))->GetRoom1()->GetID();

			} else {
				cout << "WARNING: Unkown navigation line type" << endl;
				continue;
			}

			if (IsElementInVector(rooms_ids, room_id) == false)
				continue;

			graph_file << from_door << " -> " << to_door << " [ label="
					<< from_AP->GetDistanceTo(to_AP)
					+ to_AP->GetDistanceTo(finalDestination)
					<< ", fontsize=10]; " << endl;
		}

	}
	//graph_file << "node [shape=box];  gy2; yr2; rg2; gy1; yr1; rg1;"<<endl;
	//graph_file << "node [shape=circle,fixedsize=true,width=0.9];  green2; yellow2; red2; safe2; safe1; green1; yellow1; red1;"<<endl;

	//graph_file << "0 -> 1 ;"<<endl;

	graph_file << "}" << endl;

	//done
	graph_file.close();
}

string GlobalRouter::GetRoutingInfoFile() const {

	TiXmlDocument doc(_building->GetProjectFilename());
	if (!doc.LoadFile()){
		Log->Write("ERROR: \t%s", doc.ErrorDesc());
		Log->Write("ERROR: \t could not parse the project file");
		exit(EXIT_FAILURE);
	}

	// everything is fine. proceed with parsing
	TiXmlElement* xMainNode = doc.RootElement();
	TiXmlNode* xRouters=xMainNode->FirstChild("route_choice_models");

	string nav_line_file="";

	for(TiXmlElement* e = xRouters->FirstChildElement("router"); e;
			e = e->NextSiblingElement("router")) {

		string strategy=e->Attribute("description");

		if(strategy=="local_shortest") {
			if (e->FirstChild("parameters")->FirstChildElement("navigation_lines"))
				nav_line_file=e->FirstChild("parameters")->FirstChildElement("navigation_lines")->Attribute("file");
		}
		else if(strategy=="global_shortest") {
			if (e->FirstChild("parameters")->FirstChildElement("navigation_lines"))
				nav_line_file=e->FirstChild("parameters")->FirstChildElement("navigation_lines")->Attribute("file");
		}
	}
	if (nav_line_file == "")
		return nav_line_file;
	else
		return _building->GetProjectRootDir()+nav_line_file;
}


void GlobalRouter::LoadRoutingInfos(const std::string &filename){

	if(filename=="") return;

	Log->Write("INFO:\tLoading extra routing information for the global/quickest path router");
	Log->Write("INFO:\t  from the file "+filename);

	TiXmlDocument docRouting(filename);
	if (!docRouting.LoadFile()){
		Log->Write("ERROR: \t%s", docRouting.ErrorDesc());
		Log->Write("ERROR: \t could not parse the routing file");
		exit(EXIT_FAILURE);
	}

	TiXmlElement* xRootNode = docRouting.RootElement();
	if( ! xRootNode ) {
		Log->Write("ERROR:\tRoot element does not exist");
		exit(EXIT_FAILURE);
	}

	if( xRootNode->ValueStr () != "routing" ) {
		Log->Write("ERROR:\tRoot element value is not 'routing'.");
		exit(EXIT_FAILURE);
	}

	string  version = xRootNode->Attribute("version");
	if (version != JPS_VERSION) {
		Log->Write("ERROR: \tOnly version  %d.%d supported",JPS_VERSION_MAJOR,JPS_VERSION_MINOR);
		Log->Write("ERROR: \tparsing routing file failed!");
		exit(EXIT_FAILURE);
	}

	for(TiXmlElement* xHlinesNode = xRootNode->FirstChildElement("Hlines"); xHlinesNode;
			xHlinesNode = xHlinesNode->NextSiblingElement("Hlines")) {


		for(TiXmlElement* hline = xHlinesNode->FirstChildElement("Hline"); hline;
				hline = hline->NextSiblingElement("Hline")) {

			double id = xmltof(hline->Attribute("id"), -1);
			int room_id = xmltoi(hline->Attribute("room_id"), -1);
			int subroom_id = xmltoi(hline->Attribute("subroom_id"), -1);

			double x1 = xmltof(	hline->FirstChildElement("vertex")->Attribute("px"));
			double y1 = xmltof(	hline->FirstChildElement("vertex")->Attribute("py"));
			double x2 = xmltof(	hline->LastChild("vertex")->ToElement()->Attribute("px"));
			double y2 = xmltof(	hline->LastChild("vertex")->ToElement()->Attribute("py"));

			Room* room = _building->GetRoom(room_id);
			SubRoom* subroom = room->GetSubRoom(subroom_id);

			//new implementation
			Hline* h = new Hline();
			h->SetID(id);
			h->SetPoint1(Point(x1, y1));
			h->SetPoint2(Point(x2, y2));
			h->SetRoom(room);
			h->SetSubRoom(subroom);

			_building->AddHline(h);
			subroom->AddHline(h);
		}
	}
	Log->Write("INFO:\tDone with loading extra routing information");
}
