//Call the special zone for sign detection as SignZone
/*crossing ID is special and we cant find the crossing by ID/uid, so the zone bonds are changed from crossing to transistion*/
#include "geometry/Building.h"
#include "geometry/SubRoom.h"
#include "pedestrian/Pedestrian.h"
#include "geometry/SignZone.h"
#include "geometry/NavLine.h"
#include "geometry/Transition.h"
#include "general/randomnumbergenerator.h"
#include "routing/smoke_router/NavigationGraph.h"
#include "routing/smoke_router/cognitiveMap/cognitivemap.h"
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
    std::cout<<ped->GetID()<<"ped"<<std::endl;
    bool active = false;
    std::vector<SignZone *> SignLists = building->GetSignList();
    int CurrentSubRoomID = ped->GetSubRoomID();
    int CurrentRoomID = ped->GetRoomID();
    for(std::vector<SignZone *>::iterator it_sign = SignLists.begin(); it_sign != SignLists.end(); ++it_sign){
        if((*it_sign)->GetRoomID() ==  CurrentRoomID && (*it_sign)->GetSubRoomID() == CurrentSubRoomID){
            active = true;
        }
    }
    //std::cout<<active<<"active"<<std::endl;
    if(!active){
        std::cout<<"check out"<<std::endl;
        SubRoom * sub_room = building->GetRoom(CurrentRoomID)->GetSubRoom(CurrentSubRoomID);
        GraphVertex * vertex = cognitive_map.GetGraphNetwork()->GetNavigationGraph()->operator[](sub_room);
        const GraphVertex::EdgesContainer * edges = vertex->GetAllOutEdges();
        for(GraphVertex::EdgesContainer::iterator it_edges = edges->begin(); it_edges != edges->end(); ++it_edges) {
            (*it_edges)->SetFactor(1, SignSensor::GetName());//if not in signzone, choose the nearest route
        }
    }
    //std::cout<<ped->GetSignRecord()<<"signRecord"<<std::endl;
    //std::cout<<active<<"active"<<std::endl;
    if(active && !ped->GetSignRecord()){
        //std::cout<<"2"<<std::endl;
        SubRoom * sub_room = building->GetRoom(CurrentRoomID)->GetSubRoom(CurrentSubRoomID);
        GraphVertex * vertex = cognitive_map.GetGraphNetwork()->GetNavigationGraph()->operator[](sub_room);
        const GraphVertex::EdgesContainer * edges = vertex->GetAllOutEdges();
        //bool signHeight = CurrentSignZone->GetSignHeight();//now the related probability has not been imported, the room sign is low and the t-junction is high  
        if(FirstEnteringCheck(ped)){
            std::cout<<"check in"<<std::endl;
            int targer_Transition_ID = SignSensor::GetTargetTransitionID(ped);//this is transition ID, not unique line ID
            int target_unique_ID = (building->GetTransition(targer_Transition_ID))->GetUniqueID();
            for(GraphVertex::EdgesContainer::iterator it_edges = edges->begin(); it_edges != edges->end(); ++it_edges) {
                if((*it_edges)->GetCrossing()->GetUniqueID() == target_unique_ID){
                    (*it_edges)->SetFactor(0.1, SignSensor::GetName());//for the target transition
                }else{
                    (*it_edges)->SetFactor(5.0, SignSensor::GetName());//for other transition
                }
            }
        }
        else{//for those just walking through sign zone
            //std::cout<<"pass"<<std::endl;
            SignZone * currentSignZone = GetCurrentSignZone(ped);
            if(!currentSignZone->GetSignPosition()){
                int door_unique_ID = (building->GetTransition(currentSignZone->GetOppositeTransitionID()))->GetUniqueID();
                for(GraphVertex::EdgesContainer::iterator it_edges = edges->begin(); it_edges != edges->end(); ++it_edges) {
                    if((*it_edges)->GetCrossing()->GetUniqueID() == door_unique_ID){
                        (*it_edges)->SetFactor(5.0, SignSensor::GetName());
                    }else{
                        (*it_edges)->SetFactor(0.1, SignSensor::GetName());
                    }//Todo: check whether it will set the entering transition as 0.1 and finally lead to return, yes ,it will return, think about how to solve it
                }
            }
            else{
                std::cout<<"pass T"<<std::endl;
                int random_jundgement = 0;
                std::cout<<random_jundgement<<std::endl;
                for(GraphVertex::EdgesContainer::iterator it_edges = edges->begin(); it_edges != edges->end(); ++it_edges) {
                    Hline * tran_tem = building->GetTransOrCrossByUID((*it_edges)->GetCrossing()->GetUniqueID());
                    Point ped_tem = ped->GetPos();
                    std::cout<<random_jundgement<<std::endl;
                    if(Distance(tran_tem->GetCentre(),ped_tem)<0.15){
                        (*it_edges)->SetFactor(5, SignSensor::GetName());//find the coming tran
                    }else{
                        if(random_jundgement == 0){//have not decided direction
                            RandomNumberGenerator e;
                            double ra_direction = e.GetRandomRealBetween0and1();
                            std::cout<<ra_direction<<std::endl;
                            if(ra_direction<0.5){
                                (*it_edges)->SetFactor(0.1, SignSensor::GetName());
                                random_jundgement = 1;
                            }else{
                                (*it_edges)->SetFactor(5, SignSensor::GetName());
                                random_jundgement = 2;
                            }
                            
                        }else{//have chosen direction
                            if(random_jundgement == 1){
                                (*it_edges)->SetFactor(5, SignSensor::GetName());
                            }else if(random_jundgement == 2){
                                (*it_edges)->SetFactor(0.1, SignSensor::GetName());
                            }
                        }
                    }            
                }
            }
        }
    }
};

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
    //std::cout<<"4"<<std::endl;
    SignZone * CurrentSignZone = GetCurrentSignZone(ped);
    int RoomID = ped->GetRoomID();
    int SubRoomID = ped->GetSubRoomID();
    //store all the ped inside
    std::vector<Pedestrian *> SurroundAllPed;
    std::copy_if(
        building->GetAllPedestrians().begin(),
        building->GetAllPedestrians().end(),
        std::back_inserter(SurroundAllPed),
        [RoomID, SubRoomID](auto & p) { return p->GetRoomID() == RoomID && p->GetSubRoomID() == SubRoomID; });
    std::vector<Pedestrian *> SurroundPed; //store the considered ped, except those opposite to our main person
    for(std::vector<Pedestrian *>::iterator it_SurroundAllPed = SurroundAllPed.begin();
        it_SurroundAllPed != SurroundAllPed.end(); ++it_SurroundAllPed){
        NavLine * navi = (*it_SurroundAllPed)->GetExitLine();
        if(!SignSensor::NavLine2TransitionID(navi, CurrentSignZone->GetOppositeTransitionID()))
        {
            SurroundPed.push_back(*it_SurroundAllPed);
        }
    }
    return SurroundPed;
};

//check the navline position and the Transition
bool SignSensor::NavLine2TransitionID(NavLine * navi, int tranID) const
{
    //std::cout<<"5"<<std::endl;
    std::map<int, Transition *> allTransition = building->GetAllTransitions();
    Transition * tran = building->GetTransition(tranID);
    //std::cout<<"5-1"<<std::endl;
    if((tran->GetCentre() - navi->GetCentre()).Norm() < 0.15){
        //std::cout<<"5-2"<<std::endl;
        return true;
    }else{
        //std::cout<<"5-3"<<std::endl;
        return false;
    }
};

//get surrounding people number
int SignSensor::GetSurroundingNumber(const Pedestrian * ped) const
{
    //std::cout<<"6"<<std::endl;
    std::vector<Pedestrian *> SurroundPed = GetSurroundingPeople(ped);
    return SurroundPed.size();
}

//transform the surrounding number into the SignPro column number
int SignSensor::GetSurroundingNumberRow(const Pedestrian * ped) const
{
    //std::cout<<"7"<<std::endl;
    int SurroundingNumber = GetSurroundingNumber(ped);
    if(SurroundingNumber > 1){
        SurroundingNumber = 2;
    }
    return SurroundingNumber;
};

//test if the sign is detected
bool SignSensor::DetectionTest(const Pedestrian * ped) const
{
    //std::cout<<"8"<<std::endl;
    SignZone * CurrentSignZone = GetCurrentSignZone(ped);
    int SurroundingNumberRow = GetSurroundingNumberRow(ped);
    std::vector<double> SignPro = CurrentSignZone->GetSignPro();
    /*SignPro: 3*3 -> 1*9
    first size: surrounding number; 
    second size: 0 Detection Probability, 
                 1 Follow People Probability if not detected, (surrounding number = 0, it is random probability), 
                 2 Follow People Probability if detected, (surrounding number = 0, it is follow sign probability).*/
    RandomNumberGenerator e;
    double ra_value_detection = e.GetRandomRealBetween0and1();
    if(ra_value_detection < SignPro[SurroundingNumberRow * 3 + 0]){
        return true;
    }else{
        return false;
    }
};

//get the nearest neighbor
Pedestrian * SignSensor::GetTheNearestNeighbor(const Pedestrian * ped) const
{
    //std::cout<<"9"<<std::endl;
    std::vector<double> distance;
    std::vector<Pedestrian *> SurroundPed = GetSurroundingPeople(ped);
    for(std::vector<Pedestrian *>::iterator it_SurroundPed = SurroundPed.begin(); it_SurroundPed != SurroundPed.end(); ++it_SurroundPed){
        distance.push_back((ped->GetPos() - (*it_SurroundPed)->GetPos()).Norm());
    }
    bool findSameDirectionNearestNeighbor = false;
    while (!findSameDirectionNearestNeighbor)
    {
        int order = std::distance(std::min_element(distance.begin(), distance.end()), distance.begin());
        Pedestrian * theNearestNeighbor = (*(SurroundPed.begin() + order));
        SignZone * current_sign = GetCurrentSignZone(ped);
        //std::cout<<"9-3"<<std::endl;
        if(NavLine2TransitionID(theNearestNeighbor->GetExitLine(), current_sign->GetOppositeTransitionID())){
            //std::cout<<"9-1"<<std::endl;
            distance.erase(distance.begin() + order);
            //std::cout<<"9-2"<<std::endl;
            continue;
        }else{
            findSameDirectionNearestNeighbor = true;
            return theNearestNeighbor;
        }
    }
    return 0;
}

//get the sign guidance target Transition
int SignSensor::GetTargetTransitionID(const Pedestrian * ped) const
{
    //std::cout<<"X"<<std::endl;
    SignZone * current_sign = GetCurrentSignZone(ped);
    std::vector<double> SignPro = current_sign->GetSignPro();    
    int SurroundingNumberRow = GetSurroundingNumberRow(ped);
    int DetectionColumn = 1;//not detected
    if(DetectionTest(ped)){
        DetectionColumn = 2;
    }//detected
    RandomNumberGenerator e;
    double ra_value_follow = e.GetRandomRealBetween0and1();
    int order_pro = SurroundingNumberRow * 3 + DetectionColumn;//Todo: check when add the matrix
    int target_Transition_ID;
    if(SurroundingNumberRow == 0){
        if(ra_value_follow < SignPro[order_pro]){
            target_Transition_ID = current_sign->GetSignTransitionID();//follow the sign
        }else{
            target_Transition_ID = current_sign->GetBackSignTransitionID();//backwards the sign
        }
    }else{
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
    return target_Transition_ID;
};

bool SignSensor::FirstEnteringCheck(const Pedestrian * ped) const
{
    SignZone * currentSignZone = GetCurrentSignZone(ped);
    int oldRoomID = ped->GetOldRoomID();
    std::cout<<"oldRoomID"<<oldRoomID<<std::endl;
    int oldSubRoomID = ped->GetOldSubRoomID();
    std::cout<<"oldSubRoomID"<<oldSubRoomID<<std::endl;

    int RoomID = ped->GetRoomID();
    std::cout<<"RoomID"<<RoomID<<std::endl;
    int SubRoomID = ped->GetSubRoomID();
    std::cout<<"SubRoomID"<<SubRoomID<<std::endl;

    int entering_room = currentSignZone->GetEnteringRoomID();
    std::cout<<"entering_room"<<entering_room<<std::endl;
    int entering_subroom = currentSignZone->GetEnteringSubRoomID();
    std::cout<<"entering_subroom"<<entering_subroom<<std::endl;
    if(oldRoomID == entering_room && oldSubRoomID == entering_subroom){
        return true;
    }else{
        return false;
    }
};