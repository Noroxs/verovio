/////////////////////////////////////////////////////////////////////////////
// Name:        beam.cpp
// Author:      Rodolfo Zitellini
// Created:     26/06/2012
// Copyright (c) Authors and others. All rights reserved.
/////////////////////////////////////////////////////////////////////////////

#include "beam.h"

//----------------------------------------------------------------------------

#include <array>
#include <assert.h>
#include <math.h>

//----------------------------------------------------------------------------

#include "btrem.h"
#include "doc.h"
#include "editorial.h"
#include "elementpart.h"
#include "functorparams.h"
#include "gracegrp.h"
#include "layer.h"
#include "measure.h"
#include "note.h"
#include "rest.h"
#include "smufl.h"
#include "space.h"
#include "staff.h"
#include "tuplet.h"
#include "vrv.h"

namespace vrv {

//----------------------------------------------------------------------------
// BeamSegment
//----------------------------------------------------------------------------

BeamSegment::BeamSegment()
{
    Reset();
}

BeamSegment::~BeamSegment()
{
    this->ClearCoordRefs();
}

void BeamSegment::Reset()
{
    this->ClearCoordRefs();

    m_startingX = 0;
    m_startingY = 0;
    m_beamSlope = 0.0;
    m_verticalCenter = 0;
    m_avgY = 0;
    
    m_firstNoteOrChord = NULL;
    m_lastNoteOrChord = NULL;
}

const ArrayOfBeamElementCoords *BeamSegment::GetElementCoordRefs()
{
    // this->GetList(this);
    return &m_beamElementCoordRefs;
}

void BeamSegment::ClearCoordRefs()
{
    m_beamElementCoordRefs.clear();
}

void BeamSegment::InitCoordRefs(const ArrayOfBeamElementCoords *beamElementCoords)
{
    m_beamElementCoordRefs = *beamElementCoords;
}

void BeamSegment::CalcBeam(
    Layer *layer, Staff *staff, Doc *doc, BeamDrawingInterface *beamInterface, data_BEAMPLACE place, bool init)
{
    assert(layer);
    assert(staff);
    assert(doc);

    int i, y1, y2;
    int elementCount = (int)m_beamElementCoordRefs.size();
    assert(elementCount > 0);

    // For recursive calls, avoid to re-init values
    if (init) {
        this->CalcBeamInit(layer, staff, doc, beamInterface, place);
    }

    bool horizontal = beamInterface->IsRepeatedPattern();

    // Beam@place has precedence - however, in some cases, CalcBeam is called recusively because we need to change the
    // place This occurs when mixed makes no sense and the beam is placed above or below instead.
    this->CalcBeamPlace(layer, beamInterface, place);

    // Set drawing stem positions
    for (i = 0; i < elementCount; ++i) {
        BeamElementCoord *coord = m_beamElementCoordRefs.at(i);
        if (!coord->m_stem) continue;

        if (beamInterface->m_drawingPlace == BEAMPLACE_above) {
            coord->SetDrawingStemDir(STEMDIRECTION_up, staff, doc, beamInterface);
        }
        else if (beamInterface->m_drawingPlace == BEAMPLACE_below) {
            coord->SetDrawingStemDir(STEMDIRECTION_down, staff, doc, beamInterface);
        }
        // cross-staff or beam@place=mixed
        else {
            if (beamInterface->m_crossStaff) {
                // TODO - look at staff@n and set the stem direction
                Staff *currentCrossStaff = coord->m_element->m_crossStaff;
                if (currentCrossStaff) {
                    // if (currentCrossStaff->GetN() < staff->GetN()
                }
            }
            else {
                data_STEMDIRECTION stemDir = coord->m_stem->GetStemDir();
                // TODO - Handle cases where there is no given stem direction (here we can still have NONE)
                coord->SetDrawingStemDir(stemDir, staff, doc, beamInterface);
            }
        }
    }

    ArrayOfBeamElementCoords stemUps;
    ArrayOfBeamElementCoords stemDowns;

    /*
    int halfUnit = doc->GetDrawingUnit(staff->m_drawingStaffSize) / 2;

    for (i = 0; i < elementCount; ++i) {
        BeamElementCoord *coord = m_beamElementCoordRefs.at(i);
        if (!coord->m_stem) continue;

        int stemLen = coord->m_element->GetStemmedDrawingInterface()->CalcStemLenInHalfUnits(staff);
        stemLen *= halfUnit;

        if (coord->m_stem->GetDrawingStemDir() == STEMDIRECTION_up) {
            coord->m_yBeam = coord->m_yTop + stemLen;
            coord->m_x += stemXAbove[beamInterface->m_cueSize];
            stemUps.push_back(coord);

        }
        else {
            coord->m_yBeam = coord->m_yBottom - stemLen;
            coord->m_x += stemXBelow[beamInterface->m_cueSize];
            stemDowns.push_back(coord);
        }
    }
    */

    /******************************************************************/
    // Calculate the slope doing a linear regression

    this->m_beamSlope = 0.0;
    // The vertical shift depends on the shortestDur value we have in the beam
    if (!horizontal) {
        this->CalcBeamSlope(layer, staff, doc, beamInterface);
    }

    this->m_startingX = m_beamElementCoordRefs.at(0)->m_x;
    this->m_startingY = m_beamElementCoordRefs.at(0)->m_yBeam;

    /******************************************************************/
    // Calculate the stem lengths

    /*
    // first check that the stem length is long enough (to be improved?)
    double oldYPos; // holds y position before calculation to determine if beam needs extra height
    double expectedY;
    int verticalAdjustment = 0;
    for (i = 0; i < elementCount; i++) {
        BeamElementCoord *coord = m_beamElementCoordRefs.at(i);
        if (coord->m_element->Is(REST)) {
            // Here we need to take into account the bounding box of the rest
            continue;
        }

        oldYPos = coord->m_yBeam;
        expectedY = this->m_startingY + verticalAdjustment + this->m_beamSlope * (coord->m_x - this->m_startingX);

        // if the stem is not long enough, add extra stem length needed to all members of the beam
        if ((beamInterface->m_drawingPlace == BEAMPLACE_above && (oldYPos > expectedY))
            || (beamInterface->m_drawingPlace == BEAMPLACE_below && (oldYPos < expectedY))) {
            verticalAdjustment += oldYPos - expectedY;
        }
    }

    // Now adjust the startingY position and all the elements
    this->m_startingY += verticalAdjustment;
     
    */
    for (i = 0; i < elementCount; i++) {
        BeamElementCoord *coord = m_beamElementCoordRefs.at(i);
        coord->m_yBeam = this->m_startingY + this->m_beamSlope * (coord->m_x - this->m_startingX);
    }

    /*
    // then check that the stem length reaches the center for the staff
    double minDistToCenter = -VRV_UNSET;

    for (i = 0; i < elementCount; ++i) {
        BeamElementCoord *coord = m_beamElementCoordRefs.at(i);
        if ((beamInterface->m_drawingPlace == BEAMPLACE_above)
            && (coord->m_yBeam - this->m_verticalCenter < minDistToCenter)) {
            minDistToCenter = coord->m_yBeam - this->m_verticalCenter;
        }
        else if ((beamInterface->m_drawingPlace == BEAMPLACE_below)
            && (this->m_verticalCenter - coord->m_yBeam < minDistToCenter)) {
            minDistToCenter = this->m_verticalCenter - coord->m_yBeam;
        }
    }

    if (minDistToCenter < 0) {
        this->m_startingY += (beamInterface->m_drawingPlace == BEAMPLACE_below) ? minDistToCenter : -minDistToCenter;
        for (i = 0; i < elementCount; ++i) {
            BeamElementCoord *coord = m_beamElementCoordRefs.at(i);
            coord->m_yBeam += (beamInterface->m_drawingPlace == BEAMPLACE_below) ? minDistToCenter : -minDistToCenter;
        }
    }
    */
    
    /******************************************************************/
    // Set the stem lengths

    for (i = 0; i < elementCount; ++i) {
        BeamElementCoord *coord = m_beamElementCoordRefs.at(i);
        // All notes and chords get their stem value stored
        LayerElement *el = coord->m_element;
        if ((el->Is(NOTE)) || (el->Is(CHORD))) {
            StemmedDrawingInterface *stemmedInterface = el->GetStemmedDrawingInterface();
            assert(beamInterface);

            if (beamInterface->m_drawingPlace == BEAMPLACE_above) {
                y1 = coord->m_yBeam - doc->GetDrawingStemWidth(staff->m_drawingStaffSize);
                y2 = coord->m_yBottom
                    + stemmedInterface->GetStemUpSE(doc, staff->m_drawingStaffSize, beamInterface->m_cueSize).y;
            }
            else {
                y1 = coord->m_yBeam + doc->GetDrawingStemWidth(staff->m_drawingStaffSize);
                y2 = coord->m_yTop
                    + stemmedInterface->GetStemDownNW(doc, staff->m_drawingStaffSize, beamInterface->m_cueSize).y;
            }

            Stem *stem = stemmedInterface->GetDrawingStem();
            // This is the case with fTrem on whole notes
            if (!stem) continue;

            // stem->SetDrawingStemDir(beamInterface->m_stemDir);
            // Since the value were calculated relatively to the element position, adjust them
            stem->SetDrawingXRel(coord->m_x - el->GetDrawingX());
            stem->SetDrawingYRel(y2 - el->GetDrawingY());
            stem->SetDrawingStemLen(y2 - y1);
        }
    }
}

void BeamSegment::CalcBeamInit(
    Layer *layer, Staff *staff, Doc *doc, BeamDrawingInterface *beamInterface, data_BEAMPLACE place)
{
    assert(layer);
    assert(staff);
    assert(doc);
    assert(beamInterface);

    int i, high, low;

    int elementCount = (int)m_beamElementCoordRefs.size();
    assert(elementCount > 0);

    /******************************************************************/
    // initialization

    for (i = 0; i < elementCount; ++i) {
        BeamElementCoord *coord = m_beamElementCoordRefs.at(i);
        coord->m_x = coord->m_element->GetDrawingX();
    }

    this->m_verticalCenter = staff->GetDrawingY()
        - (doc->GetDrawingDoubleUnit(staff->m_drawingStaffSize) * 2); // center point of the staff

    beamInterface->m_beamWidthBlack = doc->GetDrawingBeamWidth(staff->m_drawingStaffSize, beamInterface->m_cueSize);
    beamInterface->m_beamWidthWhite
        = doc->GetDrawingBeamWhiteWidth(staff->m_drawingStaffSize, beamInterface->m_cueSize);
    if (beamInterface->m_shortestDur == DUR_64) {
        beamInterface->m_beamWidthWhite *= 4;
        beamInterface->m_beamWidthWhite /= 3;
    }
    beamInterface->m_beamWidth = beamInterface->m_beamWidthBlack + beamInterface->m_beamWidthWhite;

    // x-offset values for stem bases, dx[y] where y = element->m_cueSize
    beamInterface->m_stemXAbove[0] = doc->GetGlyphWidth(SMUFL_E0A3_noteheadHalf, staff->m_drawingStaffSize, false)
        - (doc->GetDrawingStemWidth(staff->m_drawingStaffSize)) / 2;
    beamInterface->m_stemXBelow[1] = doc->GetGlyphWidth(SMUFL_E0A3_noteheadHalf, staff->m_drawingStaffSize, true)
        - (doc->GetDrawingStemWidth(staff->m_drawingStaffSize)) / 2;
    beamInterface->m_stemXBelow[0] = (doc->GetDrawingStemWidth(staff->m_drawingStaffSize)) / 2;
    beamInterface->m_stemXBelow[1] = (doc->GetDrawingStemWidth(staff->m_drawingStaffSize)) / 2;

    /******************************************************************/
    // Calculate the extreme values

    int yMax = 0, yMin = 0;
    int curY;
    int nbRests = 0;
    
    m_avgY = 0;
    m_nbNotesOrChords = 0;
    
    // elementCount holds the last one
    for (i = 0; i < elementCount; ++i) {
        BeamElementCoord *coord = m_beamElementCoordRefs.at(i);
        coord->m_yBeam = 0;
        
        if (coord->m_element->Is({CHORD, NOTE})) {
            if (!m_firstNoteOrChord) m_firstNoteOrChord = coord;
            m_lastNoteOrChord = coord;
            m_nbNotesOrChords++;
        }
        
        if (coord->m_element->Is(CHORD)) {
            Chord *chord = dynamic_cast<Chord *>(coord->m_element);
            assert(chord);
            chord->GetYExtremes(yMax, yMin);
            coord->m_yTop = yMax;
            coord->m_yBottom = yMin;

            this->m_avgY += ((yMax + yMin) / 2);

            // highest and lowest value;
            // high = std::max(yMax, high);
            // low = std::min(yMin, low);
        }
        else if (coord->m_element->Is(NOTE)) {
            // highest and lowest value;
            // high = std::max(coord->m_y, high);
            // low = std::min(coord->m_y, low);

            curY = coord->m_element->GetDrawingY();
            coord->m_yTop = curY;
            coord->m_yBottom = curY;
            this->m_avgY += curY;
        }
        else {
            curY = coord->m_element->GetDrawingY();
            coord->m_yTop = curY;
            coord->m_yBottom = curY;
            nbRests++;
        }
    }

    // Only if not only rests. (Will produce non-sense output anyway)
    if (elementCount != nbRests) {
        this->m_avgY /= (elementCount - nbRests);
    }
}

void BeamSegment::CalcBeamSlope(Layer *layer, Staff *staff, Doc *doc, BeamDrawingInterface *beamInterface)
{
    assert(layer);
    assert(staff);
    assert(doc);
    assert(beamInterface);
    
    m_beamSlope = 0.0;
    
    if (m_nbNotesOrChords < 2) {
        return;
    }
    assert(m_firstNoteOrChord && m_lastNoteOrChord);
    
    this->m_beamSlope = BoundingBox::CalcSlope(Point(m_firstNoteOrChord->m_x, m_firstNoteOrChord->m_yBeam), Point(m_lastNoteOrChord->m_x, m_lastNoteOrChord->m_yBeam));
    LogDebug("Slope (original) %f", m_beamSlope);
    
    if (m_beamSlope == 0.0) return;
    
    int unit = doc->GetDrawingUnit(staff->m_drawingStaffSize);
    int maxStep = unit * 4;
    int curStep = abs(m_firstNoteOrChord->m_yBeam - m_lastNoteOrChord->m_yBeam);
    
    if (m_nbNotesOrChords == 2) {
        maxStep = unit * 2;
        int dist = m_lastNoteOrChord->m_x - m_firstNoteOrChord->m_x;
        if (dist <= unit * 6) {
            dist = maxStep = unit / 2;
        }
    }
    
    // We can keep the current slope
    if (curStep < maxStep) return;

    if (beamInterface->m_drawingPlace == BEAMPLACE_above) {
        // upward
        if (m_beamSlope > 0.0) {
            m_firstNoteOrChord->m_yBeam = m_lastNoteOrChord->m_yBeam - maxStep;
        }
        else {
            m_lastNoteOrChord->m_yBeam = m_firstNoteOrChord->m_yBeam - maxStep;
        }
    }
    else if (beamInterface->m_drawingPlace == BEAMPLACE_below) {
        if (m_beamSlope > 0.0) {
            m_lastNoteOrChord->m_yBeam = m_firstNoteOrChord->m_yBeam + maxStep;
        }
        else {
            m_firstNoteOrChord->m_yBeam = m_lastNoteOrChord->m_yBeam + maxStep;
        }
    }

    this->m_beamSlope = BoundingBox::CalcSlope(Point(m_firstNoteOrChord->m_x, m_firstNoteOrChord->m_yBeam), Point(m_lastNoteOrChord->m_x, m_lastNoteOrChord->m_yBeam));
    LogDebug("Slope (adjusted) %f", m_beamSlope);
    
    /*

    double xr;

    // For slope calculation and linear regression
    double s_x = 0.0; // sum of all x(n) for n in beamElementCoord
    double s_y = 0.0; // sum of all y(n)
    double s_xy = 0.0; // sum of (x(n) * y(n))
    double s_x2 = 0.0; // sum of all x(n)^2
    double s_y2 = 0.0; // sum of all y(n)^2

    int elementCount = (int)m_beamElementCoordRefs.size();
    assert(elementCount > 0);
    int last = elementCount - 1;

    // make it relative to the first x / staff y
    int xRel = m_beamElementCoordRefs.at(0)->m_x;
    int yRel = staff->GetDrawingY();

    int i, y1;
    double verticalShiftFactor = 3.0;
    int verticalShift = ((beamInterface->m_shortestDur - DUR_8) * (beamInterface->m_beamWidth));

    // if the beam has smaller-size notes
    if (m_beamElementCoordRefs.at(last)->m_element->GetDrawingCueSize()) {
        verticalShift += doc->GetDrawingUnit(staff->m_drawingStaffSize) * 5;
    }
    else {
        verticalShift += (beamInterface->m_shortestDur > DUR_8)
            ? doc->GetDrawingDoubleUnit(staff->m_drawingStaffSize) * verticalShiftFactor
            : doc->GetDrawingDoubleUnit(staff->m_drawingStaffSize) * (verticalShiftFactor + 0.5);
    }

    // swap verticalShift direction with stem down
    if (beamInterface->m_drawingPlace == BEAMPLACE_below) {
        verticalShift = -verticalShift;
    }

    for (i = 0; i < elementCount; ++i) {
        BeamElementCoord *coord = m_beamElementCoordRefs.at(i);
        // coord->m_yBeam = coord->m_y + verticalShift;
        s_y += coord->m_yBeam - yRel;
        s_y2 += pow(coord->m_yBeam - yRel, 2);
        s_x += coord->m_x - xRel;
        s_x2 += pow(coord->m_x - xRel, 2);
        s_xy += (coord->m_x - xRel) * (coord->m_yBeam - yRel);
    }

    y1 = elementCount * s_xy - s_x * s_y;
    xr = elementCount * s_x2 - s_x * s_x;

    // Prevent division by 0
    if (y1 && xr) {
        this->m_beamSlope = y1 / xr;
    }
    else {
        this->m_beamSlope = 0.0;
    }

    //// Correction esthetique
    if (fabs(this->m_beamSlope) < doc->m_drawingBeamMinSlope) this->m_beamSlope = 0.0;
    if (fabs(this->m_beamSlope) > doc->m_drawingBeamMaxSlope)
        this->m_beamSlope = (this->m_beamSlope > 0) ? doc->m_drawingBeamMaxSlope : -doc->m_drawingBeamMaxSlope;
    ///// pente correcte: entre 0 et env 0.4 (0.2 a 0.4)
     
    */
}

void BeamSegment::CalcBeamPlace(Layer *layer, BeamDrawingInterface *beamInterface, data_BEAMPLACE place)
{
    assert(layer);
    assert(beamInterface);

    if (place != BEAMPLACE_NONE) {
        /*
        if (beamInterface->m_hasMultipleStemDir && (place != BEAMPLACE_mixed)) {
            LogDebug("Stem directions (mixed) contradict beam placement (below or above)");
        }
        else if ((beamInterface->m_notesStemDir == STEMDIRECTION_up) && (place == BEAMPLACE_below)) {
            LogDebug("Stem directions (up) contradict beam placement (below)");
        }
        else if ((beamInterface->m_notesStemDir == STEMDIRECTION_down) && (place == BEAMPLACE_above)) {
            LogDebug("Stem directions (down) contradict beam placement (above)");
        }
        */
        beamInterface->m_drawingPlace = place;
    }
    // Default with cross-staff
    else if (beamInterface->m_crossStaff) {
        beamInterface->m_drawingPlace = BEAMPLACE_mixed;
    }
    else if (beamInterface->m_hasMultipleStemDir) {
        beamInterface->m_drawingPlace = BEAMPLACE_mixed;
    }
    else {
        // Now look at the stem direction of the notes within the beam
        if (beamInterface->m_notesStemDir == STEMDIRECTION_up) {
            beamInterface->m_drawingPlace = BEAMPLACE_above;
        }
        else if (beamInterface->m_notesStemDir == STEMDIRECTION_down) {
            beamInterface->m_drawingPlace = BEAMPLACE_below;
        }
        // Look at the layer direction or, finally, at the note position
        else {
            data_STEMDIRECTION layerStemDir = layer->GetDrawingStemDir(&m_beamElementCoordRefs);
            // Layer direction ?
            if (layerStemDir == STEMDIRECTION_NONE) {
                beamInterface->m_drawingPlace
                    = (this->m_avgY < this->m_verticalCenter) ? BEAMPLACE_above : BEAMPLACE_below;
            }
            // Look at the note position
            else {
                beamInterface->m_drawingPlace = (layerStemDir == STEMDIRECTION_up) ? BEAMPLACE_above : BEAMPLACE_below;
            }
        }
    }
}

//----------------------------------------------------------------------------
// Beam
//----------------------------------------------------------------------------

Beam::Beam()
    : LayerElement("beam-"), ObjectListInterface(), BeamDrawingInterface(), AttColor(), AttBeamedWith(), AttBeamRend()
{
    RegisterAttClass(ATT_COLOR);
    RegisterAttClass(ATT_BEAMEDWITH);
    RegisterAttClass(ATT_BEAMREND);

    Reset();
}

Beam::~Beam() {}

void Beam::Reset()
{
    LayerElement::Reset();
    BeamDrawingInterface::Reset();
    ResetColor();
    ResetBeamedWith();
    ResetBeamRend();
}

void Beam::AddChild(Object *child)
{
    if (child->Is(BEAM)) {
        assert(dynamic_cast<Beam *>(child));
    }
    else if (child->Is(BTREM)) {
        assert(dynamic_cast<BTrem *>(child));
    }
    else if (child->Is(CHORD)) {
        assert(dynamic_cast<Chord *>(child));
    }
    else if (child->Is(CLEF)) {
        assert(dynamic_cast<Clef *>(child));
    }
    else if (child->Is(GRACEGRP)) {
        assert(dynamic_cast<GraceGrp *>(child));
    }
    else if (child->Is(NOTE)) {
        assert(dynamic_cast<Note *>(child));
    }
    else if (child->Is(REST)) {
        assert(dynamic_cast<Rest *>(child));
    }
    else if (child->Is(SPACE)) {
        assert(dynamic_cast<Space *>(child));
    }
    else if (child->Is(TUPLET)) {
        assert(dynamic_cast<Tuplet *>(child));
    }
    else if (child->IsEditorialElement()) {
        assert(dynamic_cast<EditorialElement *>(child));
    }
    else {
        LogError("Adding '%s' to a '%s'", child->GetClassName().c_str(), this->GetClassName().c_str());
        assert(false);
    }

    child->SetParent(this);
    m_children.push_back(child);
    Modify();
}

void Beam::FilterList(ArrayOfObjects *childList)
{
    bool firstNoteGrace = false;
    // We want to keep only notes and rests
    // Eventually, we also need to filter out grace notes properly (e.g., with sub-beams)
    ArrayOfObjects::iterator iter = childList->begin();

    while (iter != childList->end()) {
        if (!(*iter)->IsLayerElement()) {
            // remove anything that is not an LayerElement (e.g. Verse, Syl, etc)
            iter = childList->erase(iter);
            continue;
        }
        if (!(*iter)->HasInterface(INTERFACE_DURATION)) {
            // remove anything that has not a DurationInterface
            iter = childList->erase(iter);
            continue;
        }
        else {
            LayerElement *element = dynamic_cast<LayerElement *>(*iter);
            assert(element);
            // if we are at the beginning of the beam
            // and the note is cueSize
            // assume all the beam is of grace notes
            if (childList->begin() == iter) {
                if (element->IsGraceNote()) firstNoteGrace = true;
            }
            // if the first note in beam was NOT a grace
            // we have grace notes embedded in a beam
            // drop them
            if (!firstNoteGrace && element->IsGraceNote()) {
                iter = childList->erase(iter);
                continue;
            }
            // also remove notes within chords
            if (element->Is(NOTE)) {
                Note *note = dynamic_cast<Note *>(element);
                assert(note);
                if (note->IsChordTone()) {
                    iter = childList->erase(iter);
                    continue;
                }
            }
            ++iter;
        }
    }

    Staff *staff = dynamic_cast<Staff *>(this->GetFirstAncestor(STAFF));
    assert(staff);
    Staff *beamStaff = staff;
    if (this->HasBeamWith()) {
        Measure *measure = dynamic_cast<Measure *>(this->GetFirstAncestor(MEASURE));
        assert(measure);
        if (this->GetBeamWith() == OTHERSTAFF_below) {
            beamStaff = dynamic_cast<Staff *>(measure->GetNext(staff, STAFF));
            if (beamStaff == NULL) {
                LogError("Cannot access staff below for beam '%s'", this->GetUuid().c_str());
                beamStaff = staff;
            }
        }
        else if (this->GetBeamWith() == OTHERSTAFF_above) {
            beamStaff = dynamic_cast<Staff *>(measure->GetPrevious(staff, STAFF));
            if (beamStaff == NULL) {
                LogError("Cannot access staff above for beam '%s'", this->GetUuid().c_str());
                beamStaff = staff;
            }
        }
    }

    InitCoords(childList, beamStaff, this->GetPlace());
}

int Beam::GetPosition(LayerElement *element)
{
    this->GetList(this);
    int position = this->GetListIndex(element);
    // Check if this is a note in the chord
    if ((position == -1) && (element->Is(NOTE))) {
        Note *note = dynamic_cast<Note *>(element);
        assert(note);
        Chord *chord = note->IsChordTone();
        if (chord) position = this->GetListIndex(chord);
    }
    return position;
}

bool Beam::IsFirstInBeam(LayerElement *element)
{
    this->GetList(this);
    int position = this->GetPosition(element);
    // This method should be called only if the note is part of a beam
    assert(position != -1);
    // this is the first one
    if (position == 0) return true;
    return false;
}

bool Beam::IsLastInBeam(LayerElement *element)
{
    int size = (int)this->GetList(this)->size();
    int position = this->GetPosition(element);
    // This method should be called only if the note is part of a beam
    assert(position != -1);
    // this is the last one
    if (position == (size - 1)) return true;
    return false;
}

const ArrayOfBeamElementCoords *Beam::GetElementCoords()
{
    this->GetList(this);
    return &m_beamElementCoords;
}

//----------------------------------------------------------------------------
// BeamElementCoord
//----------------------------------------------------------------------------

BeamElementCoord::~BeamElementCoord() {}

//----------------------------------------------------------------------------
// Functors methods
//----------------------------------------------------------------------------

int Beam::CalcStem(FunctorParams *functorParams)
{
    CalcStemParams *params = dynamic_cast<CalcStemParams *>(functorParams);
    assert(params);

    const ArrayOfObjects *beamChildren = this->GetList(this);

    // Should we assert this at the beginning?
    if (beamChildren->empty()) {
        return FUNCTOR_CONTINUE;
    }

    this->m_beamSegment.InitCoordRefs(this->GetElementCoords());

    Layer *layer = dynamic_cast<Layer *>(this->GetFirstAncestor(LAYER));
    assert(layer);
    Staff *staff = dynamic_cast<Staff *>(layer->GetFirstAncestor(STAFF));
    assert(staff);

    this->m_beamSegment.CalcBeam(layer, staff, params->m_doc, this, this->GetPlace());

    return FUNCTOR_CONTINUE;
}

int Beam::ResetDrawing(FunctorParams *functorParams)
{
    // Call parent one too
    LayerElement::ResetDrawing(functorParams);

    this->m_beamSegment.Reset();

    // We want the list of the ObjectListInterface to be re-generated
    this->Modify();

    return FUNCTOR_CONTINUE;
}

//----------------------------------------------------------------------------
// BeamElementCoord
//----------------------------------------------------------------------------

void BeamElementCoord::SetDrawingStemDir(
    data_STEMDIRECTION stemDir, Staff *staff, Doc *doc, BeamDrawingInterface *interface)
{
    assert(staff);
    assert(doc);
    assert(interface);

    if (!this->m_stem) return;

    this->m_stem->SetDrawingStemDir(stemDir);
    this->m_onStaffLine = false;
    this->m_shortenable = 0;

    Note *note = NULL;
    if (this->m_element->Is(NOTE)) {
        note = dynamic_cast<Note *>(this->m_element);
    }

    int stemLen = 1;

    if (stemDir == STEMDIRECTION_up) {
        this->m_yBeam = this->m_yTop;
        this->m_x += interface->m_stemXAbove[interface->m_cueSize];
        if (this->m_element->Is(CHORD)) {
            Chord *chord = dynamic_cast<Chord *>(this->m_element);
            assert(chord);
            note = chord->GetTopNote();
        }
        if (note) this->m_onStaffLine = (note->GetDrawingLoc() % 2);
    }
    else {
        this->m_yBeam = this->m_yBottom;
        this->m_x += interface->m_stemXBelow[interface->m_cueSize];
        if (this->m_element->Is(CHORD)) {
            Chord *chord = dynamic_cast<Chord *>(this->m_element);
            assert(chord);
            note = chord->GetBottomNote();
        }
        if (note) this->m_onStaffLine = (note->GetDrawingLoc() % 2);
        stemLen = -1;
    }

    if (!note) return;

    bool extend = this->m_onStaffLine;
    // Check if the stem has to be shortened because outside the staff
    // In this case, Note::CalcStemLenInHalfUnits will return a value shorter than 2 * STANDARD_STEMLENGTH
    int stemLenInHalfUnits = note->CalcStemLenInHalfUnits(staff);
    // Do not extend when on the staff line
    if (stemLenInHalfUnits != STANDARD_STEMLENGTH * 2) {
        extend = false;
    }

    // For 8th notes, use the shortened stem (if shortened)
    if (this->m_dur == DUR_8) {
        if (stemLenInHalfUnits != STANDARD_STEMLENGTH * 2) {
            stemLen *= stemLenInHalfUnits;
        }
        else {
            stemLen *= (this->m_onStaffLine) ? 14 : 13;
        }
    }
    else {
        switch (this->m_dur) {
            case (DUR_16): stemLen *= (extend) ? 14 : 13; break;
            case (DUR_32): stemLen *= (extend) ? 18 : 16; break;
            case (DUR_64): stemLen *= (extend) ? 22 : 20; break;
            case (DUR_128): stemLen *= (extend) ? 26 : 24; break;
            case (DUR_256): stemLen *= (extend) ? 30 : 28; break;
            case (DUR_512): stemLen *= (extend) ? 34 : 32; break;
            case (DUR_1024): stemLen *= (extend) ? 38 : 36; break;
            default: stemLen *= 14;
        }
    }
    //this->m_shortenable = abs(stemLen - 10);
    if (stemLen % 2) {
        Note *note = dynamic_cast<Note *>(this->m_element);
        if (note) note->SetColor("red");
    }
    else {
        Note *note = dynamic_cast<Note *>(this->m_element);
        if (note) note->SetColor("");
    }
    LogMessage("%d", this->m_shortenable);
    this->m_yBeam += (stemLen * doc->GetDrawingUnit(staff->m_drawingStaffSize) / 2);
}

} // namespace vrv
