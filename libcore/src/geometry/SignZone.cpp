#include "SignZone.h"

SignZone::SignZone() {}

SignZone::~SignZone() {}


//get the configuration of SignZone
int SignZone::GetSignID(){
    return SignZone::_SignID;
}

bool SignZone::GetSignDirection(){
    return SignZone::_SignDirection;
}

/*
bool SignZone::GetSignHeight(){
    return _SignHeight;
}

*/
bool SignZone::GetSignPosition(){
    return SignZone::_SignPosition;
}

int SignZone::GetOppositeTransitionID(){
    return SignZone::_OppositeTransitionID;
}

int SignZone::GetLeftTransitionID(){
    return SignZone::_LeftTransitionID;
}

int SignZone::GetRightTransitionID(){
    return SignZone::_RightTransitionID;
}

int SignZone::GetRoomID(){
    return SignZone::_RoomID;
}

int SignZone::GetSubRoomID(){
    return SignZone::_SubRoomID;
}


int SignZone::GetEnteringSubRoomID(){
    return SignZone::_EnteringSubRoomID;
}

int SignZone::GetEnteringRoomID(){
    return SignZone::_EnteringRoomID;
}

std::string SignZone::GetCaption(){
    return SignZone::_caption;
}


int SignZone::GetSignTransitionID(){
    return SignZone::_SignTransitionID;
}

int SignZone::GetBackSignTransitionID(){
    return SignZone::_BackSignTransitionID;
}

std::vector<double> SignZone::GetSignPro(){
    return SignZone::_SignPro;
}



//set configurations from inifile
void SignZone::SetOppositeTransitionID(int op_tran_id){
    SignZone::_OppositeTransitionID = op_tran_id;
}

void SignZone::SetLeftTransitionID(int lf_tran_id){
    SignZone::_LeftTransitionID = lf_tran_id;
}

void SignZone::SetRightTransitionID(int rt_tran_id){
    SignZone::_RightTransitionID = rt_tran_id;
}

void SignZone::SetEnteringRoomID(int en_room_id){
    SignZone::_EnteringRoomID = en_room_id;
}

void SignZone::SetEnteringSubRoomID(int en_sub_id){
    SignZone::_EnteringSubRoomID = en_sub_id;
}

void SignZone::SetSignDirection(std::string sign_direction){
    if(sign_direction == "right"){
        SignZone::_SignDirection = true;
    }else if(sign_direction == "left"){
        SignZone::_SignDirection = false;
    }
}

void SignZone::SetSignPosition(std::string sign_position){
    if(sign_position == "T"){
        SignZone::_SignPosition = true;
    }else if(sign_position == "R"){
        SignZone::_SignPosition = false;
    }
}

void SignZone::SetRoomID(int room_id){
    SignZone::_RoomID = room_id;
}

void SignZone::SetSubRoomID(int SubRoom_id){
    SignZone::_SubRoomID = SubRoom_id;
}

void SignZone::SetSignID(int sign_id){
    SignZone::_SignID = sign_id;
}

void SignZone::SetCaption(std::string caption){
    SignZone::_caption = caption;
}

void SignZone::SetOthers(){
    //define the sign Transition and back sign Transition
    if(SignZone::_SignDirection){
        SignZone::_SignTransitionID = SignZone::_RightTransitionID;
        SignZone::_BackSignTransitionID = SignZone::_LeftTransitionID;
    }else{
        SignZone::_SignTransitionID = SignZone::_LeftTransitionID;
        SignZone::_BackSignTransitionID = SignZone::_RightTransitionID;
    }
    //define the sign probability
    if(SignZone::_SignPosition){
        //SignZone::_SignPro = {1,1,1,1,1,1,1,1,1};//T junction probability
        SignZone::_SignPro = {0.758, 0.483, 0.385, 0.920, 0.793, 0.850, 0.758, 0.839,0.875};
        //SignZone::_SignPro = {0, 0.483, 0.385, 0, 0.793, 0.850, 0, 0.839,0.875};
    }else{
        //SignZone::_SignPro = {0.333,0.411,0.571,0.535,0.218,0.422,0.643,0.762,0.885};//room probability, not twinkle, door top
        //SignZone::_SignPro = {1,1,1,1,1,1,1,1,1};//for test use
        //SignZone::_SignPro = {0.788, 0.460, 0.333, 0.731, 0.621, 0.941, 0.535, 0.782, 0.578};
        SignZone::_SignPro = {0.788, 0.731, 0.535, 0.460, 0.621, 0.782, 0.333, 0.941, 0.578};
        //SignZone::_SignPro = {0, 0.460, 0.333, 0, 0.621, 0.941, 0, 0.782, 0.578};
    }
}