#pragma once
#include <cstdio>
#include <map>
#include <random>
#include <vector>

class SignZone
{
protected:
    //get from inifile
    int _SignID;
    bool _SignDirection;//true-right, false-left
    //bool _SignHeight;//true-high,false-low
    bool _SignPosition;//true-Tjunction,false-room
    int _OppositeTransitionID;//the Transition opposite to the sign, only entering through this Transition is counted 
    int _LeftTransitionID;//the left direction Transition
    int _RightTransitionID;//the right direction Transition
    int _RoomID;
    int _SubRoomID; //define the signzone equal to a SubRoom, they are the same, SubRoom for moving and signzone for router
    int _EnteringSubRoomID;//only entering the sign zone through this SubRoom is considered
    int _EnteringRoomID;
    std::string _caption;

    int _SignTransitionID;//the Transition that the sign directing
    int _BackSignTransitionID;//the Transition back to the sign
    std::vector<double> _SignPro;//the probability matrix for Tjunction and room signs
    
public:
    
    SignZone();
    ~SignZone();

    int GetSignID();
    bool GetSignDirection();
    //bool GetSignHeight();
    bool GetSignPosition();
    int GetOppositeTransitionID();
    int GetLeftTransitionID();
    int GetRightTransitionID();
    int GetRoomID();
    int GetSubRoomID();
    int GetEnteringRoomID();
    int GetEnteringSubRoomID();
    std::string GetCaption();

    int GetSignTransitionID();
    int GetBackSignTransitionID();
    std::vector<double> GetSignPro();

    void SetOppositeTransitionID(int);
    void SetLeftTransitionID(int);
    void SetRightTransitionID(int);
    void SetEnteringRoomID(int);
    void SetEnteringSubRoomID(int);
    void SetSignDirection(std::string);
    void SetSignPosition(std::string);
    void SetRoomID(int);
    void SetSubRoomID(int);
    void SetSignID(int);
    void SetCaption(std::string);
    
    void SetOthers();
};