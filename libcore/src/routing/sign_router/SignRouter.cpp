/**
 * \file        SignRouter.cpp
 * \date        Feb 1, 2014
 * \version     v0.7
 * \copyright   <2009-2015> Forschungszentrum Jülich GmbH. All rights reserved.
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
 *
 *
 **/
#include "SignRouter.h"

#include "BrainStorage.h"
#include "general/Logger.h"
#include "pedestrian/Pedestrian.h"
#include "sensor/SensorManager.h"

#include <tinyxml.h>

SignRouter::SignRouter()
{
    building = nullptr;
}

SignRouter::SignRouter(int id, RoutingStrategy s) : Router(id, s)
{
    building = nullptr;
}

SignRouter::~SignRouter()
{
    delete sensor_manager;
}

int SignRouter::FindExit(Pedestrian * p)
{
    //std::cout<<"FindExit"<<std::endl;
    return SignRouter::FindDestination(p);//Yu changed 04.11.2020
    /*
    //check for former goal.
    if((*brain_storage)[p]->GetCognitiveMap().GetGraphNetwork()->HadNoDestination()) {
        sensor_manager->execute(p, SensorManager::INIT);
        return FindDestination(p);//Yu changed 01.04.2020
    }

    //Check if the Pedestrian already has a Dest. or changed subroom and needs a new one.
    else if((*brain_storage)[p]->GetCognitiveMap().GetGraphNetwork()->ChangedSubRoom()) {
        //execute periodical sensors
        sensor_manager->execute(p, SensorManager::CHANGED_ROOM);
        int status = FindDestination(p);
        (*brain_storage)[p]->GetCognitiveMap().GetGraphNetwork()->UpdateSubRoom();
        return status;
    }

    // check if ped reached a hline
    else if((*brain_storage)[p]->HlineReached()) {
        int status = FindDestination(p);
        return status;
    }

    else{
        int status = FindDestination(p);
        return status;
    }   //Yu changed 03.06.2020, add three else
    */
}

int SignRouter::FindDestination(Pedestrian * p)
{
    // Discover doors
    //std::cout<<"FindDest"<<std::endl;
    //std::cout<<"ped "<<p->GetID()<<std::endl;
    sensor_manager->execute(p, SensorManager::SIGN);//set factors
    //std::cout<<"FindDest-1"<<std::endl;
    const GraphEdge * destination = nullptr;
    //destination = (*brain_storage)[p]->GetCognitiveMap().GetGraphNetwork()->GetDestination();//这会寻找全局最优
    destination = (*brain_storage)[p]->GetCognitiveMap().GetGraphNetwork()->GetLocalDestination();
    //std::cout<<"FindDest-2"<<std::endl;
    if(destination == nullptr) {
        LOG_ERROR("Pedestrian {:d} was unable to find any destination", p->GetID());
        return -1;
    }
    //std::cout<<"FindDest-3"<<std::endl;
    (*brain_storage)[p]->GetCognitiveMap().GetGraphNetwork()->AddDestination(destination);
    const Crossing * nextTarget = destination->GetCrossing();
    const NavLine * nextNavLine = (*brain_storage)[p]->GetNextNavLine(nextTarget);
    if(nextNavLine == nullptr) {
        LOG_ERROR("No visible next subtarget found. PED {:d}", p->GetID());
        return -1;
    }
    //setting crossing to ped
    p->SetExitLine(nextNavLine);
    p->SetExitIndex(nextNavLine->GetUniqueID());
    p->AddLastDestination(nextTarget->GetID());//record used destination
    return nextNavLine->GetUniqueID();//Yu changed in 02.04.2020
}


bool SignRouter::Init(Building * b)
{
    LOG_INFO("Init SignRouter");
    building = b;

    LoadRoutingInfos(GetRoutingInfoFile());

    //Init Cognitive Map Storage, second parameter: decides whether cognitive Map is empty or complete
    if(getOptions().find("CognitiveMapFiles") == getOptions().end())
        brain_storage = std::shared_ptr<BrainStorage>(
            new BrainStorage(building, getOptions().at("CognitiveMap")[0]));
    else
        brain_storage = std::shared_ptr<BrainStorage>(new BrainStorage(
            building, getOptions().at("CognitiveMap")[0], getOptions().at("CognitiveMapFiles")[0]));
    LOG_INFO("CognitiveMapStorage initialized");

    //Init Sensor Manager
    sensor_manager = SensorManager::InitWithCertainSensors(b, brain_storage.get(), getOptions());
    LOG_INFO("SensorManager initialized");
    return true;
}


const optStorage & SignRouter::getOptions() const
{
    return options;
}

void SignRouter::addOption(const std::string & key, const std::vector<std::string> & value)
{
    options.insert(std::make_pair(key, value));
}

bool SignRouter::LoadRoutingInfos(const fs::path & filename)
{
    if(filename.empty())
        return true;

    LOG_INFO(
        "Loading extra routing information for the global/quickest path router from file {}",
        filename.string());

    TiXmlDocument docRouting(filename.string());
    if(!docRouting.LoadFile()) {
        LOG_ERROR("SignRouter, could not parse project file {}: \t%s", docRouting.ErrorDesc());
        return false;
    }

    TiXmlElement * xRootNode = docRouting.RootElement();
    if(!xRootNode) {
        LOG_ERROR("Root element not found");
        return false;
    }

    if(xRootNode->ValueStr() != "routing") {
        LOG_ERROR("Root element value is not 'routing'.");
        return false;
    }

    std::string version = xRootNode->Attribute("version");
    if(version < JPS_OLD_VERSION) {
        LOG_ERROR("Only version greater than {} supported", JPS_OLD_VERSION);
        return false;
    }
    int HlineCount = 0;
    for(TiXmlElement * xHlinesNode = xRootNode->FirstChildElement("Hlines"); xHlinesNode;
        xHlinesNode                = xHlinesNode->NextSiblingElement("Hlines")) {
        for(TiXmlElement * hline = xHlinesNode->FirstChildElement("Hline"); hline;
            hline                = hline->NextSiblingElement("Hline")) {
            double id      = xmltof(hline->Attribute("id"), -1);
            int room_id    = xmltoi(hline->Attribute("room_id"), -1);
            int subroom_id = xmltoi(hline->Attribute("subroom_id"), -1);

            double x1 = xmltof(hline->FirstChildElement("vertex")->Attribute("px"));
            double y1 = xmltof(hline->FirstChildElement("vertex")->Attribute("py"));
            double x2 = xmltof(hline->LastChild("vertex")->ToElement()->Attribute("px"));
            double y2 = xmltof(hline->LastChild("vertex")->ToElement()->Attribute("py"));

            Room * room       = building->GetRoom(room_id);
            SubRoom * subroom = room->GetSubRoom(subroom_id);

            //new implementation
            Hline * h = new Hline();
            h->SetID(id);
            h->SetPoint1(Point(x1, y1));
            h->SetPoint2(Point(x2, y2));
            h->SetRoom1(room);
            h->SetSubRoom1(subroom);

            if(building->AddHline(h)) {
                subroom->AddHline(h);
                HlineCount++;
                //h is freed in building
            } else {
                delete h;
            }
        }
    }
    LOG_INFO("Done loading extra routing information. Loaded {:d} Hlines", HlineCount);
    return true;
}

fs::path SignRouter::GetRoutingInfoFile()
{
    TiXmlDocument doc(building->GetProjectFilename().string());
    if(!doc.LoadFile()) {
        LOG_ERROR("GlobalRouter, could not parse project file: {}", doc.ErrorDesc());
        return "";
    }

    // everything is fine. proceed with parsing
    TiXmlElement * xMainNode  = doc.RootElement();
    TiXmlNode * xRouters      = xMainNode->FirstChild("route_choice_models");
    std::string nav_line_file = "";

    for(TiXmlElement * e = xRouters->FirstChildElement("router"); e;
        e                = e->NextSiblingElement("router")) {
        std::string strategy = e->Attribute("description");

        if(strategy == "smoke") {
            if(e->FirstChild("parameters")) {
                if(e->FirstChild("parameters")->FirstChildElement("navigation_lines"))
                    nav_line_file = e->FirstChild("parameters")
                                        ->FirstChildElement("navigation_lines")
                                        ->Attribute("file");
            }
        }
    }

    if(nav_line_file == "")
        return nav_line_file;
    else
        return building->GetProjectRootDir() / nav_line_file;
}
