//Call the special zone for sign detection as SignZone
/*crossing ID is special and we cant find the crossing by ID/uid, so the zone bonds are changed from crossing to transistion*/
#include "geometry/Building.h"
#include "geometry/SubRoom.h"
#include "pedestrian/Pedestrian.h"
#include "geometry/SignZone.h"
#include "geometry/NavLine.h"
#include "geometry/Transition.h"
#include "general/randomnumbergenerator.h"
#include "routing/sign_router/NavigationGraph.h"
#include "routing/sign_router/cognitiveMap/cognitivemap.h"
#include "SignSensor.h"

#include <vector>

SignSensor::~SignSensor() {}

std::string SignSensor::GetName() const
{
    return "SignSensor";
}

void SignSensor::execute(const Pedestrian * ped,  CognitiveMap & cognitive_map) const
{
    //check and get current signzone
    //std::cout<<"sign-0"<<std::endl;
    bool active = false;
    std::vector<SignZone *> SignLists = building->GetSignList();
    int CurrentSubRoomID = ped->GetSubRoomID();
    int CurrentRoomID = ped->GetRoomID();
    for(std::vector<SignZone *>::iterator it_sign = SignLists.begin(); it_sign != SignLists.end(); ++it_sign){
        if((*it_sign)->GetRoomID() ==  CurrentRoomID && (*it_sign)->GetSubRoomID() == CurrentSubRoomID){
            active = true;
        }
    }
    //std::cout<<"sign-1"<<std::endl;
    SubRoom * sub_room = building->GetRoom(CurrentRoomID)->GetSubRoom(CurrentSubRoomID);
    GraphVertex * vertex = cognitive_map.GetGraphNetwork()->GetNavigationGraph()->operator[](sub_room);
    const GraphVertex::EdgesContainer * edges = vertex->GetAllOutEdges();
    //std::cout<<"sign-2"<<std::endl;
    if(active && FirstEnteringCheck(ped)){
        //for the ped will be influenced by signs
        //std::cout<<"sign-3"<<std::endl;
        //std::cout<<"check in"<<ped->GetID()<<std::endl;
        int targer_Transition_ID = SignSensor::GetTargetTransitionID(ped);//this is transition ID, not unique line ID
        int target_unique_ID = (building->GetTransition(targer_Transition_ID))->GetUniqueID();
        //std::cout<<"target "<<targer_Transition_ID<<std::endl;
        for(GraphVertex::EdgesContainer::iterator it_edges = edges->begin(); it_edges != edges->end(); ++it_edges) {
            if((*it_edges)->GetCrossing()->GetUniqueID() == target_unique_ID){
                (*it_edges)->SetFactor(0.01, SignSensor::GetName());//for the target transition
            }else{
                (*it_edges)->SetFactor(10, SignSensor::GetName());//for other transition
            }
        }
    }else{
        //std::cout<<"sign-4"<<std::endl;
        RandomPass(ped, cognitive_map);
        //std::cout<<"pass"<<ped->GetID()<<std::endl;
    }   
};

void SignSensor::RandomPass(const Pedestrian * ped, CognitiveMap & cognitive_map) const
{
    int CurrentSubRoomID = ped->GetSubRoomID();
    int CurrentRoomID = ped->GetRoomID();
    //std::cout<<"random-0"<<std::endl;
    SubRoom * sub_room = building->GetRoom(CurrentRoomID)->GetSubRoom(CurrentSubRoomID);
    GraphVertex * vertex = cognitive_map.GetGraphNetwork()->GetNavigationGraph()->operator[](sub_room);
    //std::cout<<"random-1"<<std::endl;
    const GraphVertex::EdgesContainer * edges = vertex->GetAllOutEdges();
    //std::cout<<"random-2"<<std::endl;
    int random_jundgement = 0;
    GraphVertex::EdgesContainer * random_edges = new GraphVertex::EdgesContainer();
    //std::cout<<"random-3"<<std::endl;
    for(GraphVertex::EdgesContainer::iterator it_edges = edges->begin(); it_edges != edges->end(); ++it_edges) {
        Hline * tran_tem = building->GetTransOrCrossByUID((*it_edges)->GetCrossing()->GetUniqueID());
        Point ped_tem = ped->GetPos();
        //std::cout<<"random-3-1"<<std::endl;
        if((*it_edges)->IsExit()){
            //std::cout<<"edge Exit "<<((*it_edges)->GetCrossing()->GetID())<<std::endl;
            (*it_edges)->SetFactor(0.0001, GetName());
            continue;
        }else if(((*it_edges)->GetDest()->GetSubRoom()->GetType() == "room") || (tran_tem->DistTo(ped_tem)<0.05)){
            //std::cout<<"random-3-2"<<std::endl;
            //std::cout<<"edge Self || room"<<((*it_edges)->GetCrossing()->GetID())<<std::endl;
            (*it_edges)->SetFactor(INFINITY, GetName());//refuse coming into rooms || find the coming tran
            continue;
        }else if((find(ped->GetLastDestinations().begin(), ped->GetLastDestinations().end(), (*it_edges)->GetCrossing()->GetID())!=ped->GetLastDestinations().end())){
            //std::cout<<"visited"<<((*it_edges)->GetCrossing()->GetID())<<std::endl;
            (*it_edges)->SetFactor(100, GetName());//refuse coming into rooms || find the coming tran
            continue;
        }else{
            //std::cout<<"edge others "<<((*it_edges)->GetCrossing()->GetID())<<std::endl;
            //std::cout<<"random-3-3"<<std::endl;
            (*it_edges)->SetFactor(10, GetName());//choose one from them randomly
            //std::cout<<"random_edges"<<std::endl;
            random_edges->insert(*it_edges);
        }
    }
    //std::cout<<"random-4"<<std::endl;
    //std::cout<<random_edges<<std::endl;
    if(!random_edges->empty()){
        int random_count = 1;
        RandomNumberGenerator e;//only get the random number once here
        double ra_direction = e.GetRandomRealBetween0and1();
        for(GraphVertex::EdgesContainer::iterator rd_edges = random_edges->begin(); rd_edges != random_edges->end(); ++rd_edges) {
            if(ra_direction < random_count/random_edges->size()){
                (*rd_edges)->SetFactor(0.01, GetName());
               //std::cout<<"rd direction found!"<<std::endl;
                break;
            }
            random_count += 1;
        }
    }
    //std::cout<<"random-5"<<std::endl;
    delete random_edges;
}


//get the current sign zone of the ped
SignZone * SignSensor::GetCurrentSignZone(const Pedestrian * ped) const
{   
    std::vector<SignZone *> SignLists =  building->GetSignList();
    int CurrentSubRoomID = ped->GetSubRoomID();
    int CurrentRoomID = ped->GetRoomID();
    for(std::vector<SignZone *>::iterator it_sign = SignLists.begin(); it_sign != SignLists.end(); ++it_sign){
        if((*it_sign)->GetRoomID() ==  CurrentRoomID && (*it_sign)->GetSubRoomID() == CurrentSubRoomID){
            return (*it_sign);
            break;
        }
    }
    return 0;
};

//get surrounding people
std::vector<Pedestrian *> SignSensor::GetSurroundingPeople(const Pedestrian * ped) const
{
    SignZone * CurrentSignZone = GetCurrentSignZone(ped);
    int RoomID = ped->GetRoomID();
    int SubRoomID = ped->GetSubRoomID();
    //store all the ped inside
    std::vector<Pedestrian *> SurroundAllPed;
    std::vector<Pedestrian *> SurroundPed; //store the considered ped, except those opposite to our main person
    for(auto && others : building->GetAllPedestrians()) {
        if(others->GetRoomID() == RoomID && others->GetSubRoomID() == SubRoomID){
            //SurroundAllPed.push_back(others);
            SurroundAllPed.insert(SurroundAllPed.end(), others);
        }
    }
    //std::cout<<SurroundAllPed.size()<<"  surrounding_all_size"<<std::endl;
    if(SurroundAllPed.size()>0){
        for(std::vector<Pedestrian *>::iterator it_SurroundAllPed = SurroundAllPed.begin();
        it_SurroundAllPed != SurroundAllPed.end(); ++it_SurroundAllPed){
            NavLine * navi = (*it_SurroundAllPed)->GetExitLine();
            if(!SignSensor::NavLine2TransitionID(navi, CurrentSignZone->GetOppositeTransitionID()))
            {
                SurroundPed.push_back(*it_SurroundAllPed);
            }
        }
    }
    //std::cout<<SurroundPed.size()<<"  surrounding_size"<<std::endl;
    //std::cout<<"ped id  "<<ped->GetID()<<std::endl;
    return SurroundPed;
};

//check the navline position and the Transition
bool SignSensor::NavLine2TransitionID(NavLine * navi, int tranID) const
{
    //std::map<int, Transition *> allTransition = building->GetAllTransitions();
    Transition * tran = building->GetTransition(tranID);
    if((tran->GetCentre() - navi->GetCentre()).Norm() < 0.05){
        return true;
    }else{
        return false;
    }
};

//get surrounding people number
int SignSensor::GetSurroundingNumber(const Pedestrian * ped) const
{
    std::vector<Pedestrian *> SurroundPed = GetSurroundingPeople(ped);
    return SurroundPed.size();
}

//transform the surrounding number into the SignPro column number
int SignSensor::GetSurroundingNumberRow(const Pedestrian * ped) const
{
    int SurroundingNumber = GetSurroundingNumber(ped);
    if(SurroundingNumber > 1){
        SurroundingNumber = 2;
    }
    return SurroundingNumber;
};

//test if the sign is detected
bool SignSensor::DetectionTest(const Pedestrian * ped) const
{
    SignZone * CurrentSignZone = GetCurrentSignZone(ped);
    int SurroundingNumberRow = GetSurroundingNumberRow(ped);
    std::vector<double> SignPro = CurrentSignZone->GetSignPro();
    RandomNumberGenerator e;
    double ra_value_detection = e.GetRandomRealBetween0and1();
    //std::cout<<"random see "<<ra_value_detection<<std::endl;
    if(ra_value_detection < SignPro[SurroundingNumberRow * 3 + 0]){
        return true;
    }else{
        return false;
    }
};

//get the nearest neighbor
Pedestrian * SignSensor::GetTheNearestNeighbor(const Pedestrian * ped) const
{
    std::vector<double> distance;
    std::vector<Pedestrian *> SurroundPed = GetSurroundingPeople(ped);
    if(SurroundPed.size()>0){
        for(std::vector<Pedestrian *>::iterator it_SurroundPed = SurroundPed.begin(); it_SurroundPed != SurroundPed.end(); ++it_SurroundPed){
            distance.push_back((ped->GetPos() - (*it_SurroundPed)->GetPos()).Norm());
        }
        bool findSameDirectionNearestNeighbor = false;//avoid find the person himself
        while (!findSameDirectionNearestNeighbor)
        {
            int order = std::min_element(distance.begin(), distance.end()) - distance.begin();
            Pedestrian * theNearestNeighbor = (*(SurroundPed.begin() + order));
            SignZone * current_sign = GetCurrentSignZone(ped);
            if(NavLine2TransitionID(theNearestNeighbor->GetExitLine(), current_sign->GetOppositeTransitionID())){
                //std::cout<<"delete-opposite_ped"<<std::endl;//?
                distance.erase(distance.begin() + order);
                continue;
            }else{
                findSameDirectionNearestNeighbor = true;
                return theNearestNeighbor;
            }
        }
    }
    return 0;
}

//get the sign guidance target Transition
int SignSensor::GetTargetTransitionID(const Pedestrian * ped) const
{
    //std::cout<<"find direction"<<std::endl;
    SignZone * current_sign = GetCurrentSignZone(ped);
    std::vector<double> SignPro = current_sign->GetSignPro();
    int SurroundingNumberRow = GetSurroundingNumberRow(ped);
    int DetectionColumn = 2;//not detected
    if(DetectionTest(ped)){
        DetectionColumn = 1;
    }//detected
    RandomNumberGenerator e;
    double ra_value_follow = e.GetRandomRealBetween0and1();
    //std::cout<<"random follow "<<ra_value_follow<<std::endl;
    int order_pro = SurroundingNumberRow * 3 + DetectionColumn;
    int target_Transition_ID;
    if(DetectionColumn == 1){//detected
        if(ra_value_follow < SignPro[order_pro]){
            target_Transition_ID = current_sign->GetSignTransitionID();//follow the sign
        }else{
            target_Transition_ID = current_sign->GetBackSignTransitionID();//backwards the sign
        }
    }else{//not detected
        if(SurroundingNumberRow == 0){//no surrounding people,choose right randomly
            if(ra_value_follow<SignPro[order_pro]){
                target_Transition_ID = current_sign->GetRightTransitionID();
            }else{
                target_Transition_ID = current_sign->GetLeftTransitionID();
            }
        }else{//with surrounding people, follow people
            Pedestrian * theNearestNeighbor = GetTheNearestNeighbor(ped);
            NavLine * navi = theNearestNeighbor->GetExitLine();
            int PeopleTransitionID;
            int BackPeopleTransitionID;
            if(SignSensor::NavLine2TransitionID(navi, current_sign->GetLeftTransitionID())){
                PeopleTransitionID = current_sign->GetLeftTransitionID();
                BackPeopleTransitionID = current_sign->GetRightTransitionID();
            }
            if(SignSensor::NavLine2TransitionID(navi, current_sign->GetRightTransitionID())){
                PeopleTransitionID = current_sign->GetRightTransitionID();
                BackPeopleTransitionID = current_sign->GetLeftTransitionID();
            }
            if(ra_value_follow<SignPro[order_pro]){
                target_Transition_ID = PeopleTransitionID;//follow people
            }else{
                target_Transition_ID = BackPeopleTransitionID;//backwards people
            }
        }
    }
    //std::cout<<"ped_id "<<ped->GetID()<<" target "<<target_Transition_ID<<std::endl;
    return target_Transition_ID;
};

bool SignSensor::FirstEnteringCheck(const Pedestrian * ped) const
{
    SignZone * currentSignZone = GetCurrentSignZone(ped);
    //std::cout<<"first"<<std::endl;
    int oldRoomID = ped->GetOldRoomID();
    //std::cout<<"first-1"<<std::endl;
    int oldSubRoomID = ped->GetOldSubRoomID();
    //std::cout<<"first-2"<<std::endl;
    int entering_room = currentSignZone->GetEnteringRoomID();
    //std::cout<<"first-3"<<std::endl;
    int entering_subroom = currentSignZone->GetEnteringSubRoomID();
    //std::cout<<"first-4"<<std::endl;
    if(oldRoomID == entering_room && oldSubRoomID == entering_subroom){
        //std::cout<<"first-5"<<std::endl;
        return true;
    }else{
        //std::cout<<"first-6"<<std::endl;
        return false;
    }
};