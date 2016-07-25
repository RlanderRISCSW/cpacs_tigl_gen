#pragma once

#include "Table.h"

namespace tigl {
	// custom types in Tigl which should inherit from the generated ones (except enums)
	// maps generated types to actual tigl types
	class CustomTypesTable : public MappingTable {
	public:
		CustomTypesTable()
			: MappingTable({
				// enums
				{ "CPACSWingType_symmetry",             "TiglSymmetryAxis" },
				{ "CPACSFuselageType_symmetry",         "TiglSymmetryAxis" },
				{ "CPACSPointAbsRelType_refType",       "ECPACSTranslationType" },

				// classes
				{ "CPACSAircraftModel",                 "CCPACSModel" },
				{ "CPACSMaterialDefinition",            "CCPACSMaterial" },
				{ "CPACSFarField",                      "CCPACSFarField" },
				{ "CPACSGuideCurve",                    "CCPACSGuideCurve" },
				{ "CPACSGuideCurves",                   "CCPACSGuideCurves" },
				{ "CPACSGuideCurveProfileGeometry",     "CCPACSGuideCurveProfile" },
				{ "CPACSGuideCurveProfiles",            "CCPACSGuideCurveProfiles" },
				{ "CPACSPositioning",                   "CCPACSPositioning" },
				{ "CPACSPositionings",                  "CCPACSPositionings" },
				{ "CPACSPoint",                         "CCPACSPoint" },
				{ "CPACSPointAbsRel",                   "CCPACSPointAbsRel" },
				{ "CPACSTransformation",                "CCPACSTransformation" },
				{ "CPACSStringVectorBase",              "CCPACSStringVector" },
				{ "CPACSPointListXYZVector",            "CCPACSPointListXYZ" },

				{ "CPACSFuselage",                      "CCPACSFuselage" },
				{ "CPACSFuselages",                     "CCPACSFuselages" },
				{ "CPACSFuselageProfiles",              "CCPACSFuselageProfiles" },
				{ "CPACSFuselageSegment",               "CCPACSFuselageSegment" },
				{ "CPACSFuselageSegments",              "CCPACSFuselageSegments" },
				{ "CPACSFuselageSection",               "CCPACSFuselageSection" },
				{ "CPACSFuselageSections",              "CCPACSFuselageSections" },
				{ "CPACSFuselageElement",               "CCPACSFuselageSectionElement" },
				{ "CPACSFuselageElements",              "CCPACSFuselageSectionElements" },

				{ "CPACSComponentSegment",              "CCPACSWingComponentSegment" },
				{ "CPACSComponentSegments",             "CCPACSWingComponentSegments" },
				{ "CPACSWing",                          "CCPACSWing" },
				{ "CPACSWings",                         "CCPACSWings" },
				{ "CPACSWingCell",                      "CCPACSWingCell" },
				{ "CPACSWingCells",                     "CCPACSWingCells" },
				{ "CPACSWingElement",                   "CCPACSWingSectionElement" },
				{ "CPACSWingElements",                  "CCPACSWingSectionElements" },
				{ "CPACSWingSection",                   "CCPACSWingSection" },
				{ "CPACSWingSections",                  "CCPACSWingSections" },
				{ "CPACSWingComponentSegmentStructure", "CCPACSWingCSStructure" },
				{ "CPACSWingShell",                     "CCPACSWingShell" },
				{ "CPACSWingSegment",                   "CCPACSWingSegment" },
				{ "CPACSWingSegments",                  "CCPACSWingSegments" },
				{ "CPACSCst2D",                         "CCPACSWingProfileCST" },
		}) {}
	};
}