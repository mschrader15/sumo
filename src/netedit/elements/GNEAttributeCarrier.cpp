/****************************************************************************/
// Eclipse SUMO, Simulation of Urban MObility; see https://eclipse.dev/sumo
// Copyright (C) 2001-2025 German Aerospace Center (DLR) and others.
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License 2.0 which is available at
// https://www.eclipse.org/legal/epl-2.0/
// This Source Code may also be made available under the following Secondary
// Licenses when the conditions for such availability set forth in the Eclipse
// Public License 2.0 are satisfied: GNU General Public License, version 2
// or later which is available at
// https://www.gnu.org/licenses/old-licenses/gpl-2.0-standalone.html
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later
/****************************************************************************/
/// @file    GNEAttributeCarrier.cpp
/// @author  Jakob Erdmann
/// @date    Feb 2011
///
// Abstract Base class for gui objects which carry attributes
/****************************************************************************/

#include <netedit/GNENet.h>
#include <netedit/GNETagProperties.h>
#include <netedit/GNETagPropertiesDatabase.h>
#include <netedit/GNEViewNet.h>
#include <netedit/changes/GNEChange_Attribute.h>
#include <utils/common/StringTokenizer.h>
#include <utils/common/ToString.h>
#include <utils/emissions/PollutantsInterface.h>
#include <utils/geom/GeomConvHelper.h>
#include <utils/gui/div/GUIGlobalSelection.h>
#include <utils/gui/images/VClassIcons.h>
#include <utils/iodevices/OutputDevice.h>
#include <utils/options/OptionsCont.h>
#include <utils/shapes/PointOfInterest.h>

#include "GNEAttributeCarrier.h"

// ===========================================================================
// static members
// ===========================================================================

const std::string GNEAttributeCarrier::FEATURE_LOADED = "loaded";
const std::string GNEAttributeCarrier::FEATURE_GUESSED = "guessed";
const std::string GNEAttributeCarrier::FEATURE_MODIFIED = "modified";
const std::string GNEAttributeCarrier::FEATURE_APPROVED = "approved";
const std::string GNEAttributeCarrier::True = toString(true);
const std::string GNEAttributeCarrier::False = toString(false);

// ===========================================================================
// method definitions
// ===========================================================================

GNEAttributeCarrier::GNEAttributeCarrier(const SumoXMLTag tag, GNENet* net, const std::string& filename, const bool isTemplate) :
    myTagProperty(net->getTagPropertiesDatabase()->getTagProperty(tag)),
    myNet(net),
    myFilename(filename),
    myIsTemplate(isTemplate) {
    // check if add this AC to saving file handler
    if (isTemplate) {
        net->getSavingFilesHandler()->addTemplate(this);
    } else if (myFilename.size() > 0) {
        // add filename to saving files handler
        if (myTagProperty->isAdditionalElement()) {
            net->getSavingFilesHandler()->addAdditionalFilename(this);
        } else if (myTagProperty->isDemandElement()) {
            net->getSavingFilesHandler()->addDemandFilename(this);
        } else if (myTagProperty->isDataElement()) {
            net->getSavingFilesHandler()->addDataFilename(this);
        } else if (myTagProperty->isMeanData()) {
            net->getSavingFilesHandler()->addMeanDataFilename(this);
        }
    } else {
        // always avoid empty files
        if (myTagProperty->isAdditionalElement() && (net->getSavingFilesHandler()->getAdditionalFilenames().size() > 0)) {
            myFilename = net->getSavingFilesHandler()->getAdditionalFilenames().front();
        } else if (myTagProperty->isDemandElement() && (net->getSavingFilesHandler()->getDemandFilenames().size() > 0)) {
            myFilename = net->getSavingFilesHandler()->getDemandFilenames().front();
        } else if (myTagProperty->isDataElement() && (net->getSavingFilesHandler()->getDataFilenames().size() > 0)) {
            myFilename = net->getSavingFilesHandler()->getDataFilenames().front();
        } else if (myTagProperty->isMeanData() && (net->getSavingFilesHandler()->getMeanDataFilenames().size() > 0)) {
            myFilename = net->getSavingFilesHandler()->getMeanDataFilenames().front();
        }
    }
}


GNEAttributeCarrier::~GNEAttributeCarrier() {}


const std::string
GNEAttributeCarrier::getID() const {
    return getAttribute(SUMO_ATTR_ID);
}


GNENet*
GNEAttributeCarrier::getNet() const {
    return myNet;
}


const std::string&
GNEAttributeCarrier::getFilename() const {
    return myFilename;
}


void
GNEAttributeCarrier::changeDefaultFilename(const std::string& file) {
    if (myFilename.empty()) {
        myFilename = file;
    }
}


void
GNEAttributeCarrier::selectAttributeCarrier() {
    auto glObject = getGUIGlObject();
    if (glObject && myTagProperty->isSelectable()) {
        gSelected.select(glObject->getGlID());
        mySelected = true;
    }
}


void
GNEAttributeCarrier::unselectAttributeCarrier() {
    auto glObject = getGUIGlObject();
    if (glObject && myTagProperty->isSelectable()) {
        gSelected.deselect(glObject->getGlID());
        mySelected = false;
    }
}


bool
GNEAttributeCarrier::isAttributeCarrierSelected() const {
    return mySelected;
}


bool
GNEAttributeCarrier::drawUsingSelectColor() const {
    // first check if element is selected
    if (mySelected) {
        // get flag for network element
        const bool networkElement = myTagProperty->isNetworkElement() || myTagProperty->isAdditionalElement();
        // check current supermode
        if (networkElement && myNet->getViewNet()->getEditModes().isCurrentSupermodeNetwork()) {
            return true;
        } else if (myTagProperty->isDemandElement() && myNet->getViewNet()->getEditModes().isCurrentSupermodeDemand()) {
            return true;
        } else if (myTagProperty->isGenericData() && myNet->getViewNet()->getEditModes().isCurrentSupermodeData()) {
            return true;
        } else {
            return false;
        }
    } else {
        return false;
    }
}

void
GNEAttributeCarrier::markForDrawingFront() {
    myNet->getViewNet()->getMarkFrontElements().markAC(this);
    myDrawInFront = true;
}


void
GNEAttributeCarrier::unmarkForDrawingFront() {
    myNet->getViewNet()->getMarkFrontElements().unmarkAC(this);
    myDrawInFront = false;
}


bool
GNEAttributeCarrier::isMarkedForDrawingFront() const {
    return myDrawInFront;
}


void
GNEAttributeCarrier::drawInLayer(double typeOrLayer, const double extraOffset) const {
    if (myDrawInFront) {
        glTranslated(0, 0, GLO_FRONTELEMENT + extraOffset);
    } else {
        glTranslated(0, 0, typeOrLayer + extraOffset);
    }
}


void
GNEAttributeCarrier::setInGrid(bool value) {
    myInGrid = value;
}


bool
GNEAttributeCarrier::inGrid() const {
    return myInGrid;
}


bool
GNEAttributeCarrier::checkDrawInspectContour() const {
    return myNet->getViewNet()->getInspectedElements().isACInspected(this);
}


bool
GNEAttributeCarrier::checkDrawFrontContour() const {
    return myDrawInFront;
}


void
GNEAttributeCarrier::resetDefaultValues() {
    for (const auto& attrProperty : myTagProperty->getAttributeProperties()) {
        if (attrProperty->hasDefaultValue()) {
            setAttribute(attrProperty->getAttr(), attrProperty->getDefaultStringValue());
            if (attrProperty->isActivatable()) {
                toggleAttribute(attrProperty->getAttr(), attrProperty->getDefaultActivated());
            }
        }
    }
}


void
GNEAttributeCarrier::enableAttribute(SumoXMLAttr /*key*/, GNEUndoList* /*undoList*/) {
    throw ProcessError(TL("Nothing to enable, implement in Children"));

}


void
GNEAttributeCarrier::disableAttribute(SumoXMLAttr /*key*/, GNEUndoList* /*undoList*/) {
    throw ProcessError(TL("Nothing to disable, implement in Children"));
}


bool
GNEAttributeCarrier::isAttributeEnabled(SumoXMLAttr /*key*/) const {
    // by default, all attributes are enabled
    return true;
}


bool
GNEAttributeCarrier::isAttributeComputed(SumoXMLAttr /*key*/) const {
    // by default, all attributes aren't computed
    return false;
}


bool
GNEAttributeCarrier::hasAttribute(SumoXMLAttr key) const {
    return myTagProperty->hasAttribute(key);
}


template<> int
GNEAttributeCarrier::parse(const std::string& string) {
    if (string == "INVALID_INT") {
        return INVALID_INT;
    } else {
        return StringUtils::toInt(string);
    }
}


template<> double
GNEAttributeCarrier::parse(const std::string& string) {
    if (string == "INVALID_DOUBLE") {
        return INVALID_DOUBLE;
    } else {
        return StringUtils::toDouble(string);
    }
}


template<> SUMOTime
GNEAttributeCarrier::parse(const std::string& string) {
    return string2time(string);
}


template<> bool
GNEAttributeCarrier::parse(const std::string& string) {
    return StringUtils::toBool(string);
}


template<> std::string
GNEAttributeCarrier::parse(const std::string& string) {
    return string;
}


template<> SUMOVehicleClass
GNEAttributeCarrier::parse(const std::string& string) {
    if (string.size() == 0) {
        throw EmptyData();
    } else if (!SumoVehicleClassStrings.hasString(string)) {
        return SVC_IGNORING;
    } else {
        return SumoVehicleClassStrings.get(string);
    }
}


template<> RGBColor
GNEAttributeCarrier::parse(const std::string& string) {
    if (string.empty()) {
        return RGBColor::INVISIBLE;
    } else {
        return RGBColor::parseColor(string);
    }
}


template<> Position
GNEAttributeCarrier::parse(const std::string& string) {
    if (string.size() == 0) {
        throw EmptyData();
    } else {
        bool ok = true;
        PositionVector pos = GeomConvHelper::parseShapeReporting(string, "user-supplied position", 0, ok, false, false);
        if (!ok || (pos.size() != 1)) {
            throw NumberFormatException("(Position) " + string);
        } else {
            return pos[0];
        }
    }
}


template<> PositionVector
GNEAttributeCarrier::parse(const std::string& string) {
    PositionVector posVector;
    // empty string are allowed (It means empty position vector)
    if (string.empty()) {
        return posVector;
    } else {
        bool ok = true;
        posVector = GeomConvHelper::parseShapeReporting(string, "user-supplied shape", 0, ok, false, true);
        if (!ok) {
            throw NumberFormatException("(Position List) " + string);
        } else {
            return posVector;
        }
    }
}


template<> SUMOVehicleShape
GNEAttributeCarrier::parse(const std::string& string) {
    if ((string == "unknown") || (!SumoVehicleShapeStrings.hasString(string))) {
        return SUMOVehicleShape::UNKNOWN;
    } else {
        return SumoVehicleShapeStrings.get(string);
    }
}


template<> std::vector<std::string>
GNEAttributeCarrier::parse(const std::string& string) {
    return StringTokenizer(string).getVector();
}


template<> std::set<std::string>
GNEAttributeCarrier::parse(const std::string& string) {
    std::vector<std::string> vectorString = StringTokenizer(string).getVector();
    std::set<std::string> solution;
    for (const auto& i : vectorString) {
        solution.insert(i);
    }
    return solution;
}


template<> std::vector<int>
GNEAttributeCarrier::parse(const std::string& string) {
    std::vector<std::string> parsedValues = parse<std::vector<std::string> >(string);
    std::vector<int> parsedIntValues;
    for (const auto& i : parsedValues) {
        parsedIntValues.push_back(parse<int>(i));
    }
    return parsedIntValues;
}


template<> std::vector<double>
GNEAttributeCarrier::parse(const std::string& string) {
    std::vector<std::string> parsedValues = parse<std::vector<std::string> >(string);
    std::vector<double> parsedDoubleValues;
    for (const auto& i : parsedValues) {
        parsedDoubleValues.push_back(parse<double>(i));
    }
    return parsedDoubleValues;
}


template<> std::vector<bool>
GNEAttributeCarrier::parse(const std::string& string) {
    std::vector<std::string> parsedValues = parse<std::vector<std::string> >(string);
    std::vector<bool> parsedBoolValues;
    for (const auto& i : parsedValues) {
        parsedBoolValues.push_back(parse<bool>(i));
    }
    return parsedBoolValues;
}


template<> std::vector<SumoXMLAttr>
GNEAttributeCarrier::parse(const std::string& value) {
    // Declare string vector
    std::vector<std::string> attributesStr = GNEAttributeCarrier::parse<std::vector<std::string> > (value);
    std::vector<SumoXMLAttr> attributes;
    // Iterate over lanes IDs, retrieve Lanes and add it into parsedLanes
    for (const auto& attributeStr : attributesStr) {
        if (SUMOXMLDefinitions::Attrs.hasString(attributeStr)) {
            attributes.push_back(static_cast<SumoXMLAttr>(SUMOXMLDefinitions::Attrs.get(attributeStr)));
        } else {
            throw FormatException("Error parsing attributes. Attribute '" + attributeStr + "'  doesn't exist");
        }
    }
    return attributes;
}


template<> std::vector<GNEEdge*>
GNEAttributeCarrier::parse(GNENet* net, const std::string& value) {
    // Declare string vector
    const auto edgeIds = GNEAttributeCarrier::parse<std::vector<std::string> > (value);
    std::vector<GNEEdge*> parsedEdges;
    parsedEdges.reserve(edgeIds.size());
    // Iterate over edges IDs, retrieve Edges and add it into parsedEdges
    for (const auto& edgeID : edgeIds) {
        GNEEdge* retrievedEdge = net->getAttributeCarriers()->retrieveEdge(edgeID, false);
        if (retrievedEdge) {
            parsedEdges.push_back(net->getAttributeCarriers()->retrieveEdge(edgeID));
        } else {
            throw FormatException("Error parsing parameter " + toString(SUMO_ATTR_EDGES) + ". " +
                                  toString(SUMO_TAG_EDGE) + " '" + edgeID + "' doesn't exist");
        }
    }
    return parsedEdges;
}


template<> std::vector<GNELane*>
GNEAttributeCarrier::parse(GNENet* net, const std::string& value) {
    // Declare string vector
    const auto laneIds = GNEAttributeCarrier::parse<std::vector<std::string> > (value);
    std::vector<GNELane*> parsedLanes;
    parsedLanes.reserve(laneIds.size());
    // Iterate over lanes IDs, retrieve Lanes and add it into parsedLanes
    for (const auto& laneID : laneIds) {
        GNELane* retrievedLane = net->getAttributeCarriers()->retrieveLane(laneID, false);
        if (retrievedLane) {
            parsedLanes.push_back(net->getAttributeCarriers()->retrieveLane(laneID));
        } else {
            throw FormatException("Error parsing parameter " + toString(SUMO_ATTR_LANES) + ". " +
                                  toString(SUMO_TAG_LANE) + " '" + laneID + "'  doesn't exist");
        }
    }
    return parsedLanes;
}


template<> std::string
GNEAttributeCarrier::parseIDs(const std::vector<GNEEdge*>& ACs) {
    // obtain ID's of edges and return their join
    std::vector<std::string> edgeIDs;
    for (const auto& i : ACs) {
        edgeIDs.push_back(i->getID());
    }
    return joinToString(edgeIDs, " ");
}


template<> std::string
GNEAttributeCarrier::parseIDs(const std::vector<GNELane*>& ACs) {
    // obtain ID's of lanes and return their join
    std::vector<std::string> laneIDs;
    for (const auto& i : ACs) {
        laneIDs.push_back(i->getID());
    }
    return joinToString(laneIDs, " ");
}


bool
GNEAttributeCarrier::lanesConsecutives(const std::vector<GNELane*>& lanes) {
    // we need at least two lanes
    if (lanes.size() > 1) {
        // now check that lanes are consecutive (not necessary connected)
        int currentLane = 0;
        while (currentLane < ((int)lanes.size() - 1)) {
            int nextLane = -1;
            // iterate over outgoing edges of destination junction of edge's lane
            for (int i = 0; (i < (int)lanes.at(currentLane)->getParentEdge()->getToJunction()->getGNEOutgoingEdges().size()) && (nextLane == -1); i++) {
                // iterate over lanes of outgoing edges of destination junction of edge's lane
                for (int j = 0; (j < (int)lanes.at(currentLane)->getParentEdge()->getToJunction()->getGNEOutgoingEdges().at(i)->getChildLanes().size()) && (nextLane == -1); j++) {
                    // check if lane correspond to the next lane of "lanes"
                    if (lanes.at(currentLane)->getParentEdge()->getToJunction()->getGNEOutgoingEdges().at(i)->getChildLanes().at(j) == lanes.at(currentLane + 1)) {
                        nextLane = currentLane;
                    }
                }
            }
            if (nextLane == -1) {
                return false;
            } else {
                currentLane++;
            }
        }
        return true;
    } else {
        return false;
    }
}


template<> std::string
GNEAttributeCarrier::getACParameters() const {
    std::string result;
    // Generate an string using the following structure: "key1=value1|key2=value2|...
    for (const auto& parameter : getACParametersMap()) {
        result += parameter.first + "=" + parameter.second + "|";
    }
    // remove the last "|"
    if (!result.empty()) {
        result.pop_back();
    }
    return result;
}


template<> std::vector<std::pair<std::string, std::string> >
GNEAttributeCarrier::getACParameters() const {
    std::vector<std::pair<std::string, std::string> > result;
    // Generate a vector string using the following structure: "<key1,value1>, <key2, value2>,...
    for (const auto& parameter : getACParametersMap()) {
        result.push_back(std::make_pair(parameter.first, parameter.second));
    }
    return result;
}


void
GNEAttributeCarrier::setACParameters(const std::string& parameters, GNEUndoList* undoList) {
    // declare map
    Parameterised::Map parametersMap;
    // separate value in a vector of string using | as separator
    StringTokenizer parametersTokenizer(parameters, "|", true);
    // iterate over all values
    while (parametersTokenizer.hasNext()) {
        // obtain key and value and save it in myParameters
        const std::vector<std::string> keyValue = StringTokenizer(parametersTokenizer.next(), "=", true).getVector();
        if (keyValue.size() == 2) {
            parametersMap[keyValue.front()] = keyValue.back();
        }
    }
    // set setACParameters map
    setACParameters(parametersMap, undoList);
}


void
GNEAttributeCarrier::setACParameters(const std::vector<std::pair<std::string, std::string> >& parameters, GNEUndoList* undoList) {
    // declare parametersMap
    Parameterised::Map parametersMap;
    // Generate an string using the following structure: "key1=value1|key2=value2|...
    for (const auto& parameter : parameters) {
        parametersMap[parameter.first] = parameter.second;
    }
    // set setACParameters map
    setACParameters(parametersMap, undoList);
}


void
GNEAttributeCarrier::setACParameters(const Parameterised::Map& parameters, GNEUndoList* undoList) {
    // declare result string
    std::string paramsStr;
    // Generate an string using the following structure: "key1=value1|key2=value2|...
    for (const auto& parameter : parameters) {
        paramsStr += parameter.first + "=" + parameter.second + "|";
    }
    // remove the last "|"
    if (!paramsStr.empty()) {
        paramsStr.pop_back();
    }
    // set parameters
    setAttribute(GNE_ATTR_PARAMETERS, paramsStr, undoList);
}


void
GNEAttributeCarrier::addACParameters(const std::string& key, const std::string& attribute, GNEUndoList* undoList) {
    // get parametersMap
    Parameterised::Map parametersMap = getACParametersMap();
    // add (or update) attribute
    parametersMap[key] = attribute;
    // set attribute
    setACParameters(parametersMap, undoList);
}


void
GNEAttributeCarrier::removeACParametersKeys(const std::vector<std::string>& keepKeys, GNEUndoList* undoList) {
    // declare parametersMap
    Parameterised::Map newParametersMap;
    // iterate over parameters map
    for (const auto& parameter : getACParametersMap()) {
        // copy to newParametersMap if key is in keepKeys
        if (std::find(keepKeys.begin(), keepKeys.end(), parameter.first) != keepKeys.end()) {
            newParametersMap.insert(parameter);
        }
    }
    // set newParametersMap map
    setACParameters(newParametersMap, undoList);
}


std::string
GNEAttributeCarrier::getAlternativeValueForDisabledAttributes(SumoXMLAttr key) const {
    switch (key) {
        // Crossings
        case SUMO_ATTR_TLLINKINDEX:
        case SUMO_ATTR_TLLINKINDEX2:
            return "No TLS";
        // connections
        case SUMO_ATTR_DIR: {
            // special case for connection directions
            std::string direction = getAttribute(key);
            if (direction == "s") {
                return "Straight (s)";
            } else if (direction ==  "t") {
                return "Turn (t))";
            } else if (direction ==  "l") {
                return "Left (l)";
            } else if (direction ==  "r") {
                return "Right (r)";
            } else if (direction ==  "L") {
                return "Partially left (L)";
            } else if (direction ==  "R") {
                return "Partially right (R)";
            } else if (direction ==  "invalid") {
                return "No direction (Invalid))";
            } else {
                return "undefined";
            }
        }
        case SUMO_ATTR_STATE: {
            // special case for connection states
            std::string state = getAttribute(key);
            if (state == "-") {
                return "Dead end (-)";
            } else if (state == "=") {
                return "equal (=)";
            } else if (state == "m") {
                return "Minor link (m)";
            } else if (state == "M") {
                return "Major link (M)";
            } else if (state == "O") {
                return "TLS controller off (O)";
            } else if (state == "o") {
                return "TLS yellow flashing (o)";
            } else if (state == "y") {
                return "TLS yellow minor link (y)";
            } else if (state == "Y") {
                return "TLS yellow major link (Y)";
            } else if (state == "r") {
                return "TLS red (r)";
            } else if (state == "g") {
                return "TLS green minor (g)";
            } else if (state == "G") {
                return "TLS green major (G)";
            } else if (state == "Z") {
                return "Zipper (Z)";
            } else {
                return "undefined";
            }
        }
        default:
            return getAttribute(key);
    }
}


std::string
GNEAttributeCarrier::getAttributeForSelection(SumoXMLAttr key) const {
    return getAttribute(key);
}


const std::string&
GNEAttributeCarrier::getTagStr() const {
    return myTagProperty->getTagStr();
}


FXIcon*
GNEAttributeCarrier::getACIcon() const {
    // special case for vClass icons
    if (myTagProperty->vClassIcon()) {
        return VClassIcons::getVClassIcon(SumoVehicleClassStrings.get(getAttribute(SUMO_ATTR_VCLASS)));
    } else {
        return GUIIconSubSys::getIcon(myTagProperty->getGUIIcon());
    }
}


bool
GNEAttributeCarrier::isTemplate() const {
    return myIsTemplate;
}


const GNETagProperties*
GNEAttributeCarrier::getTagProperty() const {
    return myTagProperty;
}

// ===========================================================================
// private
// ===========================================================================

void
GNEAttributeCarrier::resetAttributes() {
    for (const auto& attrProperty : myTagProperty->getAttributeProperties()) {
        if (attrProperty->hasDefaultValue()) {
            setAttribute(attrProperty->getAttr(), attrProperty->getDefaultStringValue());
        }
    }
}


void
GNEAttributeCarrier::toggleAttribute(SumoXMLAttr /*key*/, const bool /*value*/) {
    throw ProcessError(TL("Nothing to toggle, implement in Children"));
}


std::string
GNEAttributeCarrier::getCommonAttribute(const Parameterised* parameterised, SumoXMLAttr key) const {
    switch (key) {
        case GNE_ATTR_SELECTED:
            if (mySelected) {
                return True;
            } else {
                return False;
            }
        case GNE_ATTR_FRONTELEMENT:
            if (myDrawInFront) {
                return True;
            } else {
                return False;
            }
        case GNE_ATTR_ADDITIONAL_FILE:
        case GNE_ATTR_DEMAND_FILE:
        case GNE_ATTR_DATA_FILE:
        case GNE_ATTR_MEANDATA_FILE:
            return myFilename;
        case GNE_ATTR_PARAMETERS:
            return parameterised->getParametersStr();
        default:
            throw InvalidArgument(getTagStr() + " doesn't have an attribute of type '" + toString(key) + "'");
    }
}


void
GNEAttributeCarrier::setCommonAttribute(SumoXMLAttr key, const std::string& value, GNEUndoList* undoList) {
    switch (key) {
        case GNE_ATTR_SELECTED:
        case GNE_ATTR_ADDITIONAL_FILE:
            GNEChange_Attribute::changeAttribute(this, key, value, undoList);
            // update filenames of all demand childrens
            for (auto additionalChild : getHierarchicalElement()->getChildAdditionals()) {
                additionalChild->setAttribute(key, myFilename, undoList);
            }
            break;
        case GNE_ATTR_DEMAND_FILE:
            GNEChange_Attribute::changeAttribute(this, key, value, undoList);
            // update filenames of all demand childrens
            for (auto demandChild : getHierarchicalElement()->getChildDemandElements()) {
                demandChild->setAttribute(key, myFilename, undoList);
            }
            break;
        case GNE_ATTR_DATA_FILE:
        case GNE_ATTR_MEANDATA_FILE:
        case GNE_ATTR_PARAMETERS:
            GNEChange_Attribute::changeAttribute(this, key, value, undoList);
            break;
        default:
            throw InvalidArgument(getTagStr() + " doesn't have an attribute of type '" + toString(key) + "'");
    }
}


bool
GNEAttributeCarrier::isCommonValid(SumoXMLAttr key, const std::string& value) const {
    switch (key) {
        case GNE_ATTR_SELECTED:
            return canParse<bool>(value);
        case GNE_ATTR_ADDITIONAL_FILE:
        case GNE_ATTR_DEMAND_FILE:
        case GNE_ATTR_DATA_FILE:
        case GNE_ATTR_MEANDATA_FILE:
            return SUMOXMLDefinitions::isValidFilename(value);
        case GNE_ATTR_PARAMETERS:
            return Parameterised::areParametersValid(value);
        default:
            throw InvalidArgument(getTagStr() + " doesn't have an attribute of type '" + toString(key) + "'");
    }
}


void
GNEAttributeCarrier::setCommonAttribute(Parameterised* parameterised, SumoXMLAttr key, const std::string& value) {
    switch (key) {
        case GNE_ATTR_SELECTED:
            if (parse<bool>(value)) {
                selectAttributeCarrier();
            } else {
                unselectAttributeCarrier();
            }
            break;
        case GNE_ATTR_ADDITIONAL_FILE:
            myFilename = value;
            if (value.empty()) {
                // try to avoid empty files
                if (myNet->getSavingFilesHandler()->getAdditionalFilenames().size() > 0) {
                    myFilename = myNet->getSavingFilesHandler()->getAdditionalFilenames().front();
                }
            } else {
                myNet->getSavingFilesHandler()->addAdditionalFilename(this);
            }
            break;
        case GNE_ATTR_DEMAND_FILE:
            myFilename = value;
            if (value.empty()) {
                // try to avoid empty files
                if (myNet->getSavingFilesHandler()->getDemandFilenames().size() > 0) {
                    myFilename = myNet->getSavingFilesHandler()->getDemandFilenames().front();
                }
            } else {
                myNet->getSavingFilesHandler()->addDemandFilename(this);
            }
            break;
        case GNE_ATTR_DATA_FILE:
            myFilename = value;
            if (value.empty()) {
                // try to avoid empty files
                if (myNet->getSavingFilesHandler()->getDataFilenames().size() > 0) {
                    myFilename = myNet->getSavingFilesHandler()->getDataFilenames().front();
                }
            } else {
                myNet->getSavingFilesHandler()->addDataFilename(this);
            }
            break;
        case GNE_ATTR_MEANDATA_FILE:
            myFilename = value;
            if (value.empty()) {
                // try to avoid empty files
                if (myNet->getSavingFilesHandler()->getMeanDataFilenames().size() > 0) {
                    myFilename = myNet->getSavingFilesHandler()->getMeanDataFilenames().front();
                }
            } else {
                myNet->getSavingFilesHandler()->addMeanDataFilename(this);
            }
            break;
        case GNE_ATTR_PARAMETERS:
            parameterised->setParametersStr(value);
            break;
        default:
            throw InvalidArgument(getTagStr() + " doesn't have an attribute of type '" + toString(key) + "'");
    }
}

/****************************************************************************/
