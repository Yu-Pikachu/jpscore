#pragma once
#include "AbstractSensor.h"

class SignSensor : public AbstractSensor
{
public:
    SignSensor(const Building * b) : AbstractSensor(b) {}
    ~SignSensor();

    std::string GetName() const;
    void execute(const Pedestrian *, CognitiveMap &) const;
    SignZone * GetCurrentSignZone(const Pedestrian *) const;
    std::vector<Pedestrian *> GetSurroundingPeople(const Pedestrian *) const;
    bool NavLine2TransitionID(NavLine *, int) const;
    int GetSurroundingNumber(const Pedestrian *) const;
    int GetSurroundingNumberRow(const Pedestrian *) const;
    bool DetectionTest(const Pedestrian *) const;
    Pedestrian * GetTheNearestNeighbor(const Pedestrian *) const;
    int GetTargetTransitionID(const Pedestrian *) const;
    bool FirstEnteringCheck(const Pedestrian *) const;

private:
};