//=============================================================================
// GearPairWorker.cpp
// Pure CAA C++ Standalone Spur Gear Pair Generator (CATIA V5-6R2016 / B26)
// Generates exact analytical involute gears (native GSMCurveEquations + Laws,
// zero splines / zero discrete-point fitting) and meshed CATProduct assembly.
//=============================================================================

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string>
#include <vector>
#include <windows.h>

// System & ObjectModelerBase
#include "CATSession.h"
#include "CATSessionServices.h"
#include "CATDocument.h"
#include "CATDocumentServices.h"
#include "CATInit.h"
#include "CATIDocRoots.h"
#include "CATUnicodeString.h"
#include "CATIContainerOfDocument.h"
#include "CATIAlias.h"
#include "CATError.h"
#include "CATErrorMacros.h"
#include "CATMfErrUpdate.h"

// ObjectSpecsModeler & MecModInterfaces
#include "CATISpecObject.h"
#include "CATLISTV_CATISpecObject.h"
#include "CATIDescendants.h"
#include "CATIPrtContainer.h"
#include "CATIPrtPart.h"
#include "CATIPartRequest.h"
#include "CATIPrtProceduralView.h"
#include "CATIBodyRequest.h"
#include "CATIGeometricalElement.h"
#include "CATIMf3DAxisSystemFactory.h"
#include "CATIMf3DAxisSystem.h"
#include "CATIMechanicalRootFactory.h"
#include "CATMmrInterPartCopy.h"
#include "CATMathVector.h"

// KnowledgeInterfaces
#include "CATICkeParmFactory.h"
#include "CATICkeParm.h"
#include "CATICkeInst.h"
#include "CATICkeRelation.h"
#include "CATICkeRelationExp.h"
#include "CATICkeLaw.h"
#include "CATCkeGlobalFunctions.h"
#include "CATICkeFunctionFactory.h"
#include "CATIParmPublisher.h"
#include "CATIKweActivable.h"
#include "CATICkeMagnitude.h"
#include "CATIParmDictionary.h"

// GSMInterfaces
#include "CATIGSMFactory.h"
#include "CATIGSMTool.h"
#include "CATIGSMProceduralView.h"
#include "CATIGSMPoint.h"
#include "CATIGSMPointCoord.h"
#include "CATIGSMPointOnCurve.h"
#include "CATIGSMLinePtPt.h"
#include "CATIGSMLinePtDir.h"
#include "CATIGSMDirection.h"
#include "CATIGSMCircle.h"
#include "CATIGSMCircleCtrRad.h"
#include "CATIGSMCircle3Points.h"
#include "CATIGSMSymmetry.h"
#include "CATIGSMAssemble.h"
#include "CATIGSMFill.h"
#include "CATIGSMCorner.h"
#include "CATIGSMCircleBitangentRadius.h"
#include "CATIGSMSplit.h"
#include "CATGSMOrientation.h"

// PartInterfaces
#include "CATIPrtFactory.h"
#include "CATIPad.h"
#include "CATICircPatt.h"
#include "CATIPrtLimit.h"
#include "CATLimitDefs.h"

// ProductStructure
#include "CATIProduct.h"
#include "CATIMovable.h"

// Visualization
#include "CATIVisProperties.h"
#include "CATVisPropertiesValues.h"
#include "CATShowAttribut.h"

// Mathematics & CGM Mass Properties
#include "CATMathConstant.h"
#include "CATMathDirection.h"
#include "CATMathTransformation.h"
#include "CATMathPoint.h"
#include "CATMathBox.h"
#include "CATBody.h"
#include "CATTopData.h"
#include "CATSoftwareConfiguration.h"
#include "CATICGMDynMassProperties3D.h"
#include "CATGeoFactory.h"
#include "CATGeometry.h"
#include "CATVertex.h"
#include "CATPoint.h"
#include "CATListOfCATCells.h"

struct GearSpec {
    int z;
    double m;
    double b;
    double alphaDeg;
    double x;
    double haStar;
    double cStar;
    double deltaY;
    double halfBacklash;
    double filletR;
    double d;
    double db;
    double ha;
    double hf;
    double da;
    double df;
    double s;
    double thetaEndRad;
    double psi0Rad;
};

struct GearPairParams {
    double m;
    int z1;
    int z2;
    double b1;
    double b2;
    double alphaDeg;
    double x1;
    double x2;
    double backlash;
    double stdCenterDist;
    double workAlphaRad;
    double workCenterDist;
    double deltaY;
    double contactRatio;
    double filletR;
    GearSpec g1;
    GearSpec g2;
};

struct AssemblyPlacement {
    char axis;       // 'Y' (default), 'Z', 'X'
    double p1[3];    // Gear1 center (X, Y, Z) in mm
    bool useP2;      // whether Gear2 center is specified
    double p2[3];    // Gear2 input center (X, Y, Z) in mm
    AssemblyPlacement() : axis('Y'), useP2(false) {
        p1[0] = p1[1] = p1[2] = 0.0;
        p2[0] = p2[1] = p2[2] = 0.0;
    }
};

static double InvFunc(double a) { return tan(a) - a; }

static double SolveInvAngle(double targetInv) {
    if (targetInv <= 0.0) return 0.0;
    double phi = pow(3.0 * targetInv, 1.0 / 3.0);
    if (phi > 1.3) phi = 1.3;
    for (int i = 0; i < 40; ++i) {
        double f = tan(phi) - phi - targetInv;
        if (fabs(f) < 1e-13) break;
        double t = tan(phi);
        double df = t * t;
        if (df < 1e-12) df = 1e-12;
        phi -= f / df;
        if (phi < 0.0) phi = 1e-6;
        if (phi > 1.45) phi = 1.45;
    }
    return phi;
}

static GearSpec MakeGearSpec(int z, double m, double b, double alphaDeg, double x, double deltaY, double halfBacklash, double filletR) {
    GearSpec g;
    double alpha = alphaDeg * CATPI / 180.0;
    g.z = z;
    g.m = m;
    g.b = b;
    g.alphaDeg = alphaDeg;
    g.x = x;
    g.haStar = 1.0;
    g.cStar = 0.25;
    g.deltaY = deltaY;
    g.halfBacklash = halfBacklash;
    g.filletR = filletR;
    g.d = m * z;
    g.db = g.d * cos(alpha);
    g.ha = (g.haStar + x - deltaY) * m;
    g.hf = (g.haStar + g.cStar - x) * m;
    g.da = g.d + 2.0 * g.ha;
    g.df = g.d - 2.0 * g.hf;
    g.s = m * (CATPI / 2.0 + 2.0 * x * tan(alpha)) - halfBacklash;
    g.thetaEndRad = sqrt((g.da / g.db) * (g.da / g.db) - 1.0);
    g.psi0Rad = g.s / g.d + InvFunc(alpha);
    return g;
}

static bool ComputeGearPair(double m, int z1, int z2, double b1, double b2, double alphaDeg, double x1, double x2, double backlash, double filletR, GearPairParams& out) {
    if (m <= 0.0 || z1 < 8 || z2 < 8 || b1 <= 0.0 || b2 <= 0.0 || alphaDeg <= 0.0 || alphaDeg >= 45.0 || filletR < 0.0) {
        printf("[ERROR] Invalid input parameters.\n");
        return false;
    }
    double alpha = alphaDeg * CATPI / 180.0;
    double stdA = m * (z1 + z2) / 2.0;
    double invAw = InvFunc(alpha) + 2.0 * (x1 + x2) / (double)(z1 + z2) * tan(alpha);
    if (invAw < 0.0) {
        printf("[ERROR] Total profile shift too negative; meshing equation unsolvable.\n");
        return false;
    }
    double workAlpha = SolveInvAngle(invAw);
    double workA = stdA * cos(alpha) / cos(workAlpha);
    double y = (workA - stdA) / m;
    double deltaY = (x1 + x2) - y;

    out.m = m; out.z1 = z1; out.z2 = z2; out.b1 = b1; out.b2 = b2;
    out.alphaDeg = alphaDeg; out.x1 = x1; out.x2 = x2; out.backlash = backlash;
    out.filletR = filletR;
    out.stdCenterDist = stdA;
    out.workAlphaRad = workAlpha;
    out.workCenterDist = workA;
    out.deltaY = deltaY;
    out.g1 = MakeGearSpec(z1, m, b1, alphaDeg, x1, deltaY, backlash * 0.5, filletR);
    out.g2 = MakeGearSpec(z2, m, b2, alphaDeg, x2, deltaY, backlash * 0.5, filletR);

    if (out.g1.df <= 0.0 || out.g2.df <= 0.0 || out.g1.da <= out.g1.db || out.g2.da <= out.g2.db) {
        printf("[ERROR] Invalid root or tip diameter.\n");
        return false;
    }

    double pb = CATPI * m * cos(alpha);
    double ra1 = out.g1.da * 0.5, rb1 = out.g1.db * 0.5;
    double ra2 = out.g2.da * 0.5, rb2 = out.g2.db * 0.5;
    out.contactRatio = (sqrt(ra1 * ra1 - rb1 * rb1) + sqrt(ra2 * ra2 - rb2 * rb2) - workA * sin(workAlpha)) / pb;
    return true;
}

static std::wstring BytesToWide(const char* s, UINT cp) {
    if (!s || !s[0]) return std::wstring();
    int wlen = MultiByteToWideChar(cp, 0, s, -1, NULL, 0);
    if (wlen <= 1) return std::wstring();
    std::wstring w(wlen - 1, L'\0');
    MultiByteToWideChar(cp, 0, s, -1, &w[0], wlen);
    return w;
}

static CATUnicodeString WideToCat(const wchar_t* w) {
    CATUnicodeString res;
    if (!w || !w[0]) return res;
    res.BuildFromUCChar((const CATUC2Bytes*)w, (int)wcslen(w));
    return res;
}

static std::string WideToUtf8(const wchar_t* w) {
    if (!w || !w[0]) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
    if (n <= 1) return "";
    std::string out(n - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, -1, &out[0], n, NULL, NULL);
    return out;
}

static std::string CatToUtf8(const CATUnicodeString& u) {
    const CATUC2Bytes* p = u.ConvertToUCChar();
    if (!p) return "";
    int len = 0;
    while (p[len]) ++len;
    if (len == 0) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, (const wchar_t*)p, len, NULL, 0, NULL, NULL);
    if (n <= 0) return "";
    std::string out(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, (const wchar_t*)p, len, &out[0], n, NULL, NULL);
    return out;
}

static bool TryUpdateQuiet(const CATISpecObject_var& spObj) {
    if (spObj == NULL_var) return false;
    CATTry {
        spObj->Update();
        return true;
    }
    CATCatch(CATMfErrUpdate, err) { Flush(err); return false; }
    CATCatch(CATError, err) { Flush(err); return false; }
    CATEndTry;
    return false;
}

static bool SafeUpdate(const CATISpecObject_var& spObj, const char* tag) {
    if (spObj == NULL_var) {
        printf("[ERROR] Null spec object at %s\n", tag);
        return false;
    }
    CATTry {
        spObj->Update();
        return true;
    }
    CATCatch(CATMfErrUpdate, err) {
        CATUnicodeString diag = err->GetDiagnostic();
        printf("[UPDATE FAIL %s] CATMfErrUpdate: %s\n", tag, CatToUtf8(diag).c_str());
        Flush(err);
        return false;
    }
    CATCatch(CATError, err) {
        printf("[UPDATE FAIL %s] CATError: %s\n", tag, err->GetMessageText());
        Flush(err);
        return false;
    }
    CATEndTry;
    return false;
}

static CATISpecObject_var FindShapeByAlias(const CATISpecObject_var& spTool, const wchar_t* targetName) {
    CATIDescendants_var spDesc(spTool);
    if (spDesc == NULL_var) return NULL_var;
    CATLISTV(CATISpecObject_var) children;
    spDesc->GetDirectChildren("CATISpecObject", children);
    CATUnicodeString target = WideToCat(targetName);
    for (int i = 1; i <= children.Size(); ++i) {
        CATIAlias_var spAl(children[i]);
        if (spAl != NULL_var && spAl->GetAlias() == target) {
            return children[i];
        }
    }
    return NULL_var;
}

static double GetPointRadiusXY(const CATISpecObject_var& spPtSpec) {
    CATIGeometricalElement_var spGeo(spPtSpec);
    if (spGeo == NULL_var) return -1.0;
    CATBody_var spBody = spGeo->GetBodyResult();
    if (spBody == NULL_var) return -1.0;
    CATLISTP(CATCell) cells;
    spBody->GetAllCells(cells, 0);
    if (cells.Size() < 1) return -1.0;
    CATVertex* pVtx = (CATVertex*)cells[1];
    if (!pVtx) return -1.0;
    CATPoint* pPt = pVtx->GetPoint();
    if (!pPt) return -1.0;
    double x = pPt->GetX();
    double y = pPt->GetY();
    return sqrt(x * x + y * y);
}

static bool GetWireExtremityRadiiXY(const CATISpecObject_var& spWireSpec,
                                    double& x0, double& y0, double& x1, double& y1) {
    CATIGeometricalElement_var spGeo(spWireSpec);
    if (spGeo == NULL_var) return false;
    CATBody_var spBody = spGeo->GetBodyResult();
    if (spBody == NULL_var) return false;
    CATLISTP(CATCell) verts;
    spBody->GetAllCells(verts, 0);
    if (verts.Size() < 2) return false;
    CATVertex* vA = (CATVertex*)verts[1];
    CATVertex* vB = (CATVertex*)verts[verts.Size()];
    if (!vA || !vB) return false;
    CATPoint* pA = vA->GetPoint();
    CATPoint* pB = vB->GetPoint();
    if (!pA || !pB) return false;
    x0 = pA->GetX(); y0 = pA->GetY();
    x1 = pB->GetX(); y1 = pB->GetY();
    return true;
}

static CATISpecObject_var FindCornerByRadii(const CATIGSMFactory_var& spGSMFact, const CATISpecObject_var& spWireSet,
                                            const CATISpecObject_var& e1, const CATISpecObject_var& e2,
                                            const CATISpecObject_var& spSupport, const CATICkeParm_var& pR,
                                            double filletR, double targetR, double refTheta,
                                            bool expectOuterFromTarget, const wchar_t* alias) {
    const CATGSMOrientation orients[2] = { CATGSMSameOrientation, CATGSMInvertOrientation };
    for (int idx = 1; idx <= 2; ++idx) {
        for (int i = 0; i < 2; ++i) {
            for (int j = 0; j < 2; ++j) {
                for (int ti = 0; ti < 2; ++ti) {
                    for (int tj = 0; tj < 2; ++tj) {
                        CATIGSMCircleBitangentRadius_var spTan = spGSMFact->CreateTangentCircle(e1, e2, spSupport, pR, orients[i], orients[j]);
                        if (spTan == NULL_var) continue;
                        spTan->SetFirstTangentOrientation(orients[ti]);
                        spTan->SetSecondTangentOrientation(orients[tj]);
                        spTan->SetDiscriminationIndex(idx);
                        CATIGSMProceduralView_var(spTan)->InsertInProceduralView(spWireSet);
                        if (TryUpdateQuiet(spTan)) {
                            double x0, y0, x1, y1;
                            if (GetWireExtremityRadiiXY(spTan, x0, y0, x1, y1)) {
                                double rA = sqrt(x0 * x0 + y0 * y0);
                                double rB = sqrt(x1 * x1 + y1 * y1);
                                double chord = sqrt((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0));
                                double boxDiag = 999.0;
                                CATIGeometricalElement_var spGeo(spTan);
                                CATBody_var spB = (spGeo != NULL_var) ? spGeo->GetBodyResult() : NULL_var;
                                if (spB != NULL_var) {
                                    CATMathBox mb = spB->GetBoundingBox();
                                    double bx0, bx1, by0, by1, bz0, bz1;
                                    mb.GetLimits(bx0, bx1, by0, by1, bz0, bz1);
                                    boxDiag = sqrt((bx1 - bx0) * (bx1 - bx0) + (by1 - by0) * (by1 - by0));
                                }
                                if (y0 > 0.0 && y1 > 0.0 && chord <= 2.15 * filletR && boxDiag <= 2.15 * filletR) {
                                    double xCirc, yCirc, rCirc, xFlk, yFlk, rFlk;
                                    if ((expectOuterFromTarget && rA <= rB) || (!expectOuterFromTarget && rA >= rB)) {
                                        xCirc = x0; yCirc = y0; rCirc = rA;
                                        xFlk  = x1; yFlk  = y1; rFlk  = rB;
                                    } else {
                                        xCirc = x1; yCirc = y1; rCirc = rB;
                                        xFlk  = x0; yFlk  = y0; rFlk  = rA;
                                    }
                                    double thCirc = atan2(yCirc, xCirc);
                                    double thFlk  = atan2(yFlk, xFlk);
                                    bool match = false;
                                    if (expectOuterFromTarget) {
                                        match = (fabs(rCirc - targetR) < 0.03 &&
                                                 rFlk > targetR + 0.05 * filletR &&
                                                 thCirc > refTheta + 1e-5 &&
                                                 thFlk < thCirc);
                                    } else {
                                        match = (fabs(rCirc - targetR) < 0.03 &&
                                                 rFlk < targetR - 0.05 * filletR &&
                                                 thCirc > 0.0 && thCirc < refTheta - 1e-5 &&
                                                 thFlk > thCirc);
                                    }
                                    if (match) {
                                        printf("  [Corner OK %s] circ=(%.3f,%.3f r=%.4f th=%.4f) flk=(%.3f,%.3f r=%.4f th=%.4f) refTh=%.4f diag=%.4f\n",
                                               expectOuterFromTarget ? "Root" : "Tip",
                                               xCirc, yCirc, rCirc, thCirc, xFlk, yFlk, rFlk, thFlk, refTheta, boxDiag);
                                        CATIAlias_var(spTan)->SetAlias(WideToCat(alias));
                                        return CATISpecObject_var(spTan);
                                    }
                                }
                            }
                        }
                        CATIDescendants_var(spWireSet)->RemoveChild(CATISpecObject_var(spTan));
                    }
                }
            }
        }
    }
    printf("[ERROR] Could not resolve corner orientation for fillet R=%.3f near targetR=%.3f (refTh=%.4f).\n", filletR, targetR, refTheta);
    return NULL_var;
}

static void AddFormula2(const CATICkeParmFactory_var& spRelFact, const CATIParmPublisher_var& spRelPub,
                        const char* fName, const CATICkeParm_var& pOut,
                        const CATICkeParm_var& p1, const CATICkeParm_var& p2, const char* expr) {
    CATCkeListOf(Parm) lst;
    lst.Append(p1);
    if (p2 != NULL_var) lst.Append(p2);
    CATICkeRelation_var spRel = spRelFact->CreateFormula(CATUnicodeString(fName), "", "", pOut, &lst, expr, NULL_var, CATCke::False);
    if (spRel != NULL_var && spRelPub != NULL_var) spRelPub->Append(spRel);
}

static void AddFormula3(const CATICkeParmFactory_var& spRelFact, const CATIParmPublisher_var& spRelPub,
                        const char* fName, const CATICkeParm_var& pOut,
                        const CATICkeParm_var& p1, const CATICkeParm_var& p2, const CATICkeParm_var& p3, const char* expr) {
    CATCkeListOf(Parm) lst;
    lst.Append(p1); lst.Append(p2); lst.Append(p3);
    CATICkeRelation_var spRel = spRelFact->CreateFormula(CATUnicodeString(fName), "", "", pOut, &lst, expr, NULL_var, CATCke::False);
    if (spRel != NULL_var && spRelPub != NULL_var) spRelPub->Append(spRel);
}

static void AddFormula4(const CATICkeParmFactory_var& spRelFact, const CATIParmPublisher_var& spRelPub,
                        const char* fName, const CATICkeParm_var& pOut,
                        const CATICkeParm_var& p1, const CATICkeParm_var& p2, const CATICkeParm_var& p3, const CATICkeParm_var& p4, const char* expr) {
    CATCkeListOf(Parm) lst;
    lst.Append(p1); lst.Append(p2); lst.Append(p3); lst.Append(p4);
    CATICkeRelation_var spRel = spRelFact->CreateFormula(CATUnicodeString(fName), "", "", pOut, &lst, expr, NULL_var, CATCke::False);
    if (spRel != NULL_var && spRelPub != NULL_var) spRelPub->Append(spRel);
}

static bool VerifyMainBodySolid(const CATIPrtPart_var& spPart, double expectedRa, double expectedB);

static bool BuildSingleGearDoc(const wchar_t* seedPath, const wchar_t* workPath, const GearSpec& spec, const wchar_t* bodyName,
                               bool saveAndClose, CATDocument*& outDoc, CATIPrtContainer*& outCont, CATISpecObject_var& outMainBody) {
    outDoc = NULL;
    outCont = NULL;
    outMainBody = NULL_var;
    printf("--- Building Gear (z=%d, m=%.3f, x=%.3f, b=%.3f, R=%.3f) -> %s ---\n",
           spec.z, spec.m, spec.x, spec.b, spec.filletR, WideToUtf8(workPath).c_str());

    if (!CopyFileW(seedPath, workPath, FALSE)) {
        printf("[ERROR] CopyFileW failed from %s to %s (err=%lu)\n",
               WideToUtf8(seedPath).c_str(), WideToUtf8(workPath).c_str(), GetLastError());
        return false;
    }
    SetFileAttributesW(workPath, FILE_ATTRIBUTE_NORMAL);

    CATDocument* pDoc = NULL;
    CATUnicodeString uOutPath = WideToCat(workPath);
    HRESULT hr = CATDocumentServices::OpenDocument(uOutPath, pDoc);
    if (FAILED(hr) || !pDoc) {
        printf("[ERROR] OpenDocument failed: hr=0x%08X\n", (unsigned int)hr);
        return false;
    }

    bool ok = false;
    CATInit_var spInit(pDoc);
    CATIPrtContainer* piPrtCont = (spInit != NULL_var) ? (CATIPrtContainer*)spInit->GetRootContainer("CATIPrtContainer") : NULL;
    if (!piPrtCont) {
        printf("[ERROR] Failed to retrieve CATIPrtContainer.\n");
        CATDocumentServices::Remove(*pDoc);
        return false;
    }

    do {
        CATIPrtPart_var spPart = piPrtCont->GetPart();
        CATIGSMFactory_var spGSMFact(piPrtCont);
        CATIPrtFactory_var spPrtFact(piPrtCont);
        CATIParmPublisher_var spPartPub(spPart);
        if (spPart == NULL_var || spGSMFact == NULL_var || spPrtFact == NULL_var || spPartPub == NULL_var) {
            printf("[ERROR] Missing Part/GSM/Prt/Publisher interfaces.\n");
            break;
        }

        // Rename MainBody
        CATIPartRequest_var spPartReq(spPart);
        CATBaseUnknown_var spMainBodyBU;
        if (spPartReq != NULL_var) spPartReq->GetMainBody("MfDefault3DView", spMainBodyBU);
        CATISpecObject_var spMainBody(spMainBodyBU);
        CATIAlias_var spBodyAlias(spMainBody);
        if (spBodyAlias != NULL_var) spBodyAlias->SetAlias(WideToCat(bodyName));

        // 1. Find ParameterSet & RelationSet, remove non-Law formulas
        CATISpecObject_var spParmSet = CATCkeGlobalFunctions::GetFunctionFactory()->GetCurrentSet(CATICkeFunctionFactory::Parameter, spPartPub, CATCke::True);
        CATISpecObject_var spRelSet = CATCkeGlobalFunctions::GetFunctionFactory()->GetCurrentSet(CATICkeFunctionFactory::Relation, spPartPub, CATCke::True);
        CATIParmPublisher_var spParmPub(spParmSet);
        CATIParmPublisher_var spRelPub(spRelSet);
        CATICkeParmFactory_var spParmFact = spParmSet->GetFeatContainer();
        CATICkeParmFactory_var spRelFact = spRelSet->GetFeatContainer();

        CATLISTV(CATISpecObject_var) oldRels;
        spRelPub->GetDirectChildren("CATICkeRelation", oldRels);
        CATICkeRelation_var spLawX, spLawY, spLawZ;
        for (int i = 1; i <= oldRels.Size(); ++i) {
            CATIAlias_var spAl(oldRels[i]);
            CATUnicodeString rName = (spAl != NULL_var) ? spAl->GetAlias() : CATUnicodeString("");
            if (rName == "LogSpiral_X_StandardEquation") spLawX = oldRels[i];
            else if (rName == "LogSpiral_Y_StandardEquation") spLawY = oldRels[i];
            else if (rName == "LogSpiral_Z_StandardEquation") spLawZ = oldRels[i];
            else spRelPub->RemoveChild(oldRels[i]);
        }
        if (spLawX == NULL_var || spLawY == NULL_var || spLawZ == NULL_var) {
            printf("[ERROR] Missing native StandardEquation Laws in seed.\n");
            break;
        }

        // 2. Create Chinese Gear Parameters (note: CreateLength takes meters, CreateAngle takes radians!)
        CATICkeParm_var pM = spParmFact->CreateLength(WideToCat(L"模数m"), spec.m / 1000.0);
        CATICkeParm_var pZ = spParmFact->CreateInteger(WideToCat(L"齿数z"), spec.z);
        CATICkeParm_var pB = spParmFact->CreateLength(WideToCat(L"齿宽b"), spec.b / 1000.0);
        CATICkeParm_var pAlpha = spParmFact->CreateAngle(WideToCat(L"压力角α"), spec.alphaDeg * CATPI / 180.0);
        CATICkeParm_var pX = spParmFact->CreateReal(WideToCat(L"变位系数x"), spec.x);
        CATICkeParm_var pHaStar = spParmFact->CreateReal(WideToCat(L"齿顶高系数ha"), spec.haStar);
        CATICkeParm_var pCStar = spParmFact->CreateReal(WideToCat(L"顶隙系数c"), spec.cStar);
        CATICkeParm_var pDeltaY = spParmFact->CreateReal(WideToCat(L"齿顶高变动系数Δy"), spec.deltaY);
        CATICkeParm_var pBacklash = spParmFact->CreateLength(WideToCat(L"单轮齿厚减薄量"), spec.halfBacklash / 1000.0);

        CATICkeParm_var pD = spParmFact->CreateLength(WideToCat(L"分度圆直径d"), spec.d / 1000.0);
        CATICkeParm_var pHa = spParmFact->CreateLength(WideToCat(L"齿顶高ha"), spec.ha / 1000.0);
        CATICkeParm_var pHf = spParmFact->CreateLength(WideToCat(L"齿根高hf"), spec.hf / 1000.0);
        CATICkeParm_var pDa = spParmFact->CreateLength(WideToCat(L"齿顶圆直径da"), spec.da / 1000.0);
        CATICkeParm_var pDf = spParmFact->CreateLength(WideToCat(L"齿根圆直径df"), spec.df / 1000.0);
        CATICkeParm_var pDb = spParmFact->CreateLength(WideToCat(L"基圆直径db"), spec.db / 1000.0);
        CATICkeParm_var pS = spParmFact->CreateLength(WideToCat(L"分度圆齿厚s"), spec.s / 1000.0);
        CATICkeParm_var pThetaEnd = spParmFact->CreateAngle(WideToCat(L"渐开线终止角"), spec.thetaEndRad);
        CATICkeParm_var pPsi0 = spParmFact->CreateAngle(WideToCat(L"齿形偏置角"), spec.psi0Rad);

        // Helper parameters for GSM features
        CATICkeParm_var pHalfDa = spParmFact->CreateLength(WideToCat(L"齿顶圆半径ra"), (spec.da * 0.5) / 1000.0);
        CATICkeParm_var pHalfDf = spParmFact->CreateLength(WideToCat(L"齿根圆半径rf"), (spec.df * 0.5) / 1000.0);
        CATICkeParm_var pZeroLen = spParmFact->CreateLength("ZeroLen", 0.0);
        CATICkeParm_var pTenMm = spParmFact->CreateLength("AxisLen", 0.010);
        CATICkeParm_var pAngStep = spParmFact->CreateAngle(WideToCat(L"齿距角"), 2.0 * CATPI / spec.z);
        CATICkeParm_var pFilletR = spParmFact->CreateLength(WideToCat(L"倒角半径R"), spec.filletR / 1000.0);

        CATICkeParm_var allParms[] = {
            pM, pZ, pB, pAlpha, pX, pHaStar, pCStar, pDeltaY, pBacklash, pFilletR,
            pD, pHa, pHf, pDa, pDf, pDb, pS, pThetaEnd, pPsi0,
            pHalfDa, pHalfDf, pAngStep
        };
        for (int i = 0; i < (int)(sizeof(allParms) / sizeof(allParms[0])); ++i) {
            spParmPub->Append(allParms[i]);
        }

        // 3. Create Formulas using positional parameters (CATCke::False -> locale/backtick safe!)
        AddFormula2(spRelFact, spRelPub, "Formula_d", pD, pM, pZ, "a1*a2");
        AddFormula4(spRelFact, spRelPub, "Formula_ha", pHa, pHaStar, pX, pDeltaY, pM, "(a1+a2-a3)*a4");
        AddFormula4(spRelFact, spRelPub, "Formula_hf", pHf, pHaStar, pCStar, pX, pM, "(a1+a2-a3)*a4");
        AddFormula2(spRelFact, spRelPub, "Formula_da", pDa, pD, pHa, "a1+2*a2");
        AddFormula2(spRelFact, spRelPub, "Formula_df", pDf, pD, pHf, "a1-2*a2");
        AddFormula2(spRelFact, spRelPub, "Formula_db", pDb, pD, pAlpha, "a1*cos(a2)");
        AddFormula4(spRelFact, spRelPub, "Formula_s", pS, pM, pX, pAlpha, pBacklash, "PI/2*a1+2*a2*a1*tan(a3)-a4");
        AddFormula2(spRelFact, spRelPub, "Formula_ra", pHalfDa, pDa, NULL_var, "a1/2");
        AddFormula2(spRelFact, spRelPub, "Formula_rf", pHalfDf, pDf, NULL_var, "a1/2");
        AddFormula2(spRelFact, spRelPub, "Formula_thetaEnd", pThetaEnd, pDa, pDb, "sqrt((a1/a2)*(a1/a2)-1)*1rad");
        AddFormula3(spRelFact, spRelPub, "Formula_psi0", pPsi0, pS, pD, pAlpha, "(a1/a2)*1rad+(tan(a3)*1rad-a3)");
        AddFormula2(spRelFact, spRelPub, "Formula_angStep", pAngStep, pZ, NULL_var, "360deg/a1");

        // 4. Locate native GSMCurveEquations in LogSpiral_Equation and modify the 3 native Laws
        CATISpecObject_var spWireSet;
        CATLISTV(CATISpecObject_var) partChildren;
        CATIDescendants_var(spPart)->GetDirectChildren("CATIGSMTool", partChildren);
        for (int i = 1; i <= partChildren.Size(); ++i) {
            CATIAlias_var spAl(partChildren[i]);
            if (spAl != NULL_var && spAl->GetAlias() == "LogSpiral_Equation") {
                spWireSet = partChildren[i];
                spAl->SetAlias(WideToCat(L"齿轮线框"));
                break;
            }
        }
        if (spWireSet == NULL_var) {
            printf("[ERROR] LogSpiral_Equation GSMTool not found in seed.\n");
            break;
        }
        CATISpecObject_var spInvR = FindShapeByAlias(spWireSet, L"LogSpiral_Standard_Equation");
        if (spInvR == NULL_var) {
            printf("[ERROR] LogSpiral_Standard_Equation not found.\n");
            break;
        }
        CATIAlias_var(spInvR)->SetAlias(WideToCat(L"渐开线_右"));

        // Rewrite each Law using its own formal Out/In parameters + a3=pDb, a4=pThetaEnd, a5=pPsi0
        CATICkeRelation_var laws[3] = {spLawX, spLawY, spLawZ};
        for (int idx = 0; idx < 3; ++idx) {
            CATICkeLaw_var spLaw(laws[idx]);
            CATCkeListOfParm outParms = laws[idx]->OutParameters();
            CATCkeListOfParm formalParms = spLaw->GetFormalParameters();
            if (!outParms || outParms->Size() < 1 || !formalParms || formalParms->Size() < 2) {
                printf("[ERROR] Law %d missing formal parameters.\n", idx);
                return false;
            }
            CATICkeParm_var pOutFormal = (*outParms)[1];
            CATICkeParm_var pInFormal;
            for (int k = 1; k <= formalParms->Size(); ++k) {
                CATICkeParm_var cand = (*formalParms)[k];
                if (cand != pOutFormal) { pInFormal = cand; break; }
            }
            CATCkeListOf(Parm) lawList;
            lawList.Append(pOutFormal); // a1
            lawList.Append(pInFormal);  // a2
            lawList.Append(pDb);        // a3
            lawList.Append(pThetaEnd);  // a4
            lawList.Append(pPsi0);      // a5

            CATICkeRelationExp_var spExp(laws[idx]);
            CATCke::Boolean modOk = CATCke::False;
            if (idx == 0) {
                modOk = spExp->Modify(&lawList, "a1 = (a3/2)*(cos(a5-a4*a2)-((a4*a2)/1rad)*sin(a5-a4*a2))", NULL_var, CATCke::False);
            } else if (idx == 1) {
                modOk = spExp->Modify(&lawList, "a1 = (a3/2)*(sin(a5-a4*a2)+((a4*a2)/1rad)*cos(a5-a4*a2))", NULL_var, CATCke::False);
            } else {
                modOk = spExp->Modify(&lawList, "a1 = 0mm*a2", NULL_var, CATCke::False);
            }
            if (!modOk) {
                printf("[ERROR] Law %d Modify() returned False.\n", idx);
                return false;
            }
            CATIKweActivable_var spAct(laws[idx]);
            if (spAct != NULL_var) spAct->SetActivationStatus(1);
        }

        if (!SafeUpdate(spInvR, "Update_Involute_Right")) break;

        // 5. Build Closed Single-Tooth Wire via native CATIGSMFactory
        CATLISTV(CATISpecObject_var) refPlanes = spPart->GetReferencePlanes();
        for (int rp = 1; rp <= refPlanes.Size(); ++rp) {
            CATIAlias_var al(refPlanes[rp]);
            printf("  RefPlane[%d] = %s\n", rp, al != NULL_var ? CatToUtf8(al->GetAlias()).c_str() : "?");
        }
        CATISpecObject_var spXYPlane = refPlanes[1];
        CATISpecObject_var spZXPlane = refPlanes[3];

        CATIGSMPointCoord_var spCenterPt = spGSMFact->CreatePoint(pZeroLen, pZeroLen, pZeroLen);
        CATIGSMProceduralView_var(spCenterPt)->InsertInProceduralView(spWireSet);
        CATIAlias_var(spCenterPt)->SetAlias(WideToCat(L"齿轮中心"));
        if (!SafeUpdate(spCenterPt, "Update_CenterPt")) break;

        CATIGSMSymmetry_var spInvL = spGSMFact->CreateSymmetry(spInvR, spZXPlane);
        CATIGSMProceduralView_var(spInvL)->InsertInProceduralView(spWireSet);
        CATIAlias_var(spInvL)->SetAlias(WideToCat(L"渐开线_左"));
        if (!SafeUpdate(spInvL, "Update_Involute_Left")) break;

        // Extremities of right involute, verify inner vs outer by radius
        CATIGSMPointOnCurve_var spPt0 = spGSMFact->CreatePoint(spInvR, NULL_var, NULL_var, CATGSMSameOrientation);
        CATIGSMProceduralView_var(spPt0)->InsertInProceduralView(spWireSet);
        CATIGSMPointOnCurve_var spPt1 = spGSMFact->CreatePoint(spInvR, NULL_var, NULL_var, CATGSMInvertOrientation);
        CATIGSMProceduralView_var(spPt1)->InsertInProceduralView(spWireSet);
        if (!SafeUpdate(spPt0, "Update_Pt0") || !SafeUpdate(spPt1, "Update_Pt1")) break;

        double r0 = GetPointRadiusXY(spPt0);
        double r1 = GetPointRadiusXY(spPt1);
        printf("  Involute endpoints: r0=%.4f mm, r1=%.4f mm (expected rb=%.4f, ra=%.4f)\n",
               r0, r1, spec.db * 0.5, spec.da * 0.5);
        CATISpecObject_var spPtBaseR = (r0 <= r1) ? CATISpecObject_var(spPt0) : CATISpecObject_var(spPt1);
        CATISpecObject_var spPtTipR  = (r0 <= r1) ? CATISpecObject_var(spPt1) : CATISpecObject_var(spPt0);

        CATIGSMSymmetry_var spPtTipL = spGSMFact->CreateSymmetry(spPtTipR, spZXPlane);
        CATIGSMProceduralView_var(spPtTipL)->InsertInProceduralView(spWireSet);
        CATIGSMSymmetry_var spPtBaseL = spGSMFact->CreateSymmetry(spPtBaseR, spZXPlane);
        CATIGSMProceduralView_var(spPtBaseL)->InsertInProceduralView(spWireSet);

        CATIGSMPointCoord_var spPtTipMid = spGSMFact->CreatePoint(pHalfDa, pZeroLen, pZeroLen);
        CATIGSMProceduralView_var(spPtTipMid)->InsertInProceduralView(spWireSet);
        if (!SafeUpdate(spPtTipL, "Update_TipL") || !SafeUpdate(spPtBaseL, "Update_BaseL") || !SafeUpdate(spPtTipMid, "Update_TipMid")) break;

        // Root circle (created before corners so root fillet can tangent to it)
        CATIGSMCircleCtrRad_var spRootCircle = spGSMFact->CreateCircle(spCenterPt, spXYPlane, FALSE, pHalfDf, NULL_var, NULL_var);
        CATIGSMCircle_var(spRootCircle)->SetLimitation(CATIGSMCircle::Whole);
        CATIGSMProceduralView_var(spRootCircle)->InsertInProceduralView(spWireSet);
        CATIAlias_var(spRootCircle)->SetAlias(WideToCat(L"齿根圆"));
        if (!SafeUpdate(spRootCircle, "Update_RootCircle")) break;

        CATISpecObject_var spToothWireObj;
        if (spec.filletR > 1e-4) {
            CATIGSMCircleCtrRad_var spTipCircle = spGSMFact->CreateCircle(spCenterPt, spXYPlane, FALSE, pHalfDa, NULL_var, NULL_var);
            CATIGSMCircle_var(spTipCircle)->SetLimitation(CATIGSMCircle::Whole);
            CATIGSMProceduralView_var(spTipCircle)->InsertInProceduralView(spWireSet);
            CATIGSMLinePtPt_var spSegR = spGSMFact->CreateLine(spCenterPt, spPtBaseR);
            CATIGSMProceduralView_var(spSegR)->InsertInProceduralView(spWireSet);
            if (!SafeUpdate(spTipCircle, "Update_TipCircle") || !SafeUpdate(spSegR, "Update_SegR")) break;

            double alphaTip = acos(spec.db / spec.da);
            double thetaTip = spec.psi0Rad - InvFunc(alphaTip);
            double thetaRoot = (spec.df <= spec.db) ? spec.psi0Rad : (spec.psi0Rad - InvFunc(acos(spec.db / spec.df)));

            CATISpecObject_var spTipCornerR = FindCornerByRadii(
                spGSMFact, spWireSet, spInvR, spTipCircle, spXYPlane, pFilletR,
                spec.filletR, spec.da * 0.5, thetaTip, false, L"齿顶倒圆_右");
            CATISpecObject_var spRootFlankRef = (spec.df <= spec.db) ? CATISpecObject_var(spSegR) : CATISpecObject_var(spInvR);
            CATISpecObject_var spRootCornerR = FindCornerByRadii(
                spGSMFact, spWireSet, spRootFlankRef, spRootCircle, spXYPlane, pFilletR,
                spec.filletR, spec.df * 0.5, thetaRoot, true, L"齿根倒圆_右");
            if (spTipCornerR == NULL_var || spRootCornerR == NULL_var) break;

            CATLISTV(CATISpecObject_var) flankElems;
            flankElems.Append(spSegR);
            flankElems.Append(spInvR);
            CATIGSMAssemble_var spRightFlank = spGSMFact->CreateAssemble(flankElems);
            CATIGSMProceduralView_var(spRightFlank)->InsertInProceduralView(spWireSet);
            if (!SafeUpdate(spRightFlank, "Update_RightFlank")) break;

            // Tip corner endpoints: inner on RightFlank, outer on TipArc
            CATIGSMPointOnCurve_var spTp0 = spGSMFact->CreatePoint(spTipCornerR, NULL_var, NULL_var, CATGSMSameOrientation);
            CATIGSMPointOnCurve_var spTp1 = spGSMFact->CreatePoint(spTipCornerR, NULL_var, NULL_var, CATGSMInvertOrientation);
            CATIGSMProceduralView_var(spTp0)->InsertInProceduralView(spWireSet);
            CATIGSMProceduralView_var(spTp1)->InsertInProceduralView(spWireSet);
            // Root corner endpoints: inner on RootCircle, outer on RightFlank
            CATIGSMPointOnCurve_var spRp0 = spGSMFact->CreatePoint(spRootCornerR, NULL_var, NULL_var, CATGSMSameOrientation);
            CATIGSMPointOnCurve_var spRp1 = spGSMFact->CreatePoint(spRootCornerR, NULL_var, NULL_var, CATGSMInvertOrientation);
            CATIGSMProceduralView_var(spRp0)->InsertInProceduralView(spWireSet);
            CATIGSMProceduralView_var(spRp1)->InsertInProceduralView(spWireSet);
            if (!SafeUpdate(spTp0, "Update_Tp0") || !SafeUpdate(spTp1, "Update_Tp1") ||
                !SafeUpdate(spRp0, "Update_Rp0") || !SafeUpdate(spRp1, "Update_Rp1")) break;

            double tr0 = GetPointRadiusXY(spTp0), tr1 = GetPointRadiusXY(spTp1);
            CATISpecObject_var spPtFlankTipR = (tr0 <= tr1) ? CATISpecObject_var(spTp0) : CATISpecObject_var(spTp1);
            CATISpecObject_var spPtTipTanR   = (tr0 <= tr1) ? CATISpecObject_var(spTp1) : CATISpecObject_var(spTp0);

            double rr0 = GetPointRadiusXY(spRp0), rr1 = GetPointRadiusXY(spRp1);
            CATISpecObject_var spPtRootTanR   = (rr0 <= rr1) ? CATISpecObject_var(spRp0) : CATISpecObject_var(spRp1);
            CATISpecObject_var spPtFlankRootR = (rr0 <= rr1) ? CATISpecObject_var(spRp1) : CATISpecObject_var(spRp0);

            // Split RightFlank at RootTan -> keep upper half reaching ra
            CATIGSMSplit_var spSplit1 = spGSMFact->CreateSplit(spRightFlank, spPtFlankRootR, CATGSMSameOrientation);
            CATIGSMProceduralView_var(spSplit1)->InsertInProceduralView(spWireSet);
            if (!SafeUpdate(spSplit1, "Update_FlankSplit1")) break;
            {
                double sx0, sy0, sx1, sy1;
                if (GetWireExtremityRadiiXY(spSplit1, sx0, sy0, sx1, sy1)) {
                    double rMax = (sqrt(sx0 * sx0 + sy0 * sy0) > sqrt(sx1 * sx1 + sy1 * sy1))
                                  ? sqrt(sx0 * sx0 + sy0 * sy0) : sqrt(sx1 * sx1 + sy1 * sy1);
                    if (rMax < spec.da * 0.5 - 0.05) {
                        spSplit1->SetOrientation(CATGSMInvertOrientation);
                        if (!SafeUpdate(spSplit1, "Update_FlankSplit1_Flip")) break;
                    }
                }
            }

            // Split upper flank at TipTan -> keep middle segment [PtFlankRootR .. PtFlankTipR]
            CATIGSMSplit_var spSplit2 = spGSMFact->CreateSplit(spSplit1, spPtFlankTipR, CATGSMSameOrientation);
            CATIGSMProceduralView_var(spSplit2)->InsertInProceduralView(spWireSet);
            if (!SafeUpdate(spSplit2, "Update_FlankSplit2")) break;
            {
                double sx0, sy0, sx1, sy1;
                if (GetWireExtremityRadiiXY(spSplit2, sx0, sy0, sx1, sy1)) {
                    double rMax = (sqrt(sx0 * sx0 + sy0 * sy0) > sqrt(sx1 * sx1 + sy1 * sy1))
                                  ? sqrt(sx0 * sx0 + sy0 * sy0) : sqrt(sx1 * sx1 + sy1 * sy1);
                    if (rMax > spec.da * 0.5 - 0.02 * spec.filletR) {
                        spSplit2->SetOrientation(CATGSMInvertOrientation);
                        if (!SafeUpdate(spSplit2, "Update_FlankSplit2_Flip")) break;
                    }
                }
            }

            CATIGSMSymmetry_var spPtTipTanL = spGSMFact->CreateSymmetry(spPtTipTanR, spZXPlane);
            CATIGSMProceduralView_var(spPtTipTanL)->InsertInProceduralView(spWireSet);
            if (!SafeUpdate(spPtTipTanL, "Update_TipTanL")) break;

            CATIGSMCircle3Points_var spTipArcTrim = spGSMFact->CreateCircle(spPtTipTanR, spPtTipMid, spPtTipTanL);
            CATIGSMCircle_var(spTipArcTrim)->SetLimitation(CATIGSMCircle::Trimmed);
            CATIGSMProceduralView_var(spTipArcTrim)->InsertInProceduralView(spWireSet);
            if (!SafeUpdate(spTipArcTrim, "Update_TipArcTrim")) break;

            CATIGSMLinePtPt_var spSegRootCenterR = spGSMFact->CreateLine(spCenterPt, spPtRootTanR);
            CATIGSMProceduralView_var(spSegRootCenterR)->InsertInProceduralView(spWireSet);
            if (!SafeUpdate(spSegRootCenterR, "Update_SegRootCenterR")) break;

            CATIGSMSymmetry_var spTipCornerL = spGSMFact->CreateSymmetry(spTipCornerR, spZXPlane);
            CATIGSMSymmetry_var spFlankTrimL = spGSMFact->CreateSymmetry(spSplit2, spZXPlane);
            CATIGSMSymmetry_var spRootCornerL = spGSMFact->CreateSymmetry(spRootCornerR, spZXPlane);
            CATIGSMSymmetry_var spSegRootCenterL = spGSMFact->CreateSymmetry(spSegRootCenterR, spZXPlane);
            CATIGSMProceduralView_var(spTipCornerL)->InsertInProceduralView(spWireSet);
            CATIGSMProceduralView_var(spFlankTrimL)->InsertInProceduralView(spWireSet);
            CATIGSMProceduralView_var(spRootCornerL)->InsertInProceduralView(spWireSet);
            CATIGSMProceduralView_var(spSegRootCenterL)->InsertInProceduralView(spWireSet);
            if (!SafeUpdate(spTipCornerL, "Update_TipCornerL") || !SafeUpdate(spFlankTrimL, "Update_FlankTrimL") ||
                !SafeUpdate(spRootCornerL, "Update_RootCornerL") || !SafeUpdate(spSegRootCenterL, "Update_SegRootCenterL")) break;

            CATLISTV(CATISpecObject_var) wireElems;
            wireElems.Append(spSegRootCenterR);
            wireElems.Append(spRootCornerR);
            wireElems.Append(spSplit2);
            wireElems.Append(spTipCornerR);
            wireElems.Append(spTipArcTrim);
            wireElems.Append(spTipCornerL);
            wireElems.Append(spFlankTrimL);
            wireElems.Append(spRootCornerL);
            wireElems.Append(spSegRootCenterL);
            CATIGSMAssemble_var spToothWire = spGSMFact->CreateAssemble(wireElems);
            CATIGSMProceduralView_var(spToothWire)->InsertInProceduralView(spWireSet);
            CATIAlias_var(spToothWire)->SetAlias(WideToCat(L"单齿封闭轮廓"));
            if (!SafeUpdate(spToothWire, "Update_ToothWire")) break;
            spToothWireObj = CATISpecObject_var(spToothWire);
        } else {
            CATIGSMCircle3Points_var spTipArc = spGSMFact->CreateCircle(spPtTipR, spPtTipMid, spPtTipL);
            CATIGSMCircle_var(spTipArc)->SetLimitation(CATIGSMCircle::Trimmed);
            CATIGSMProceduralView_var(spTipArc)->InsertInProceduralView(spWireSet);
            if (!SafeUpdate(spTipArc, "Update_TipArc")) break;

            CATIGSMLinePtPt_var spSegR = spGSMFact->CreateLine(spCenterPt, spPtBaseR);
            CATIGSMProceduralView_var(spSegR)->InsertInProceduralView(spWireSet);
            CATIGSMLinePtPt_var spSegL = spGSMFact->CreateLine(spPtBaseL, spCenterPt);
            CATIGSMProceduralView_var(spSegL)->InsertInProceduralView(spWireSet);
            if (!SafeUpdate(spSegR, "Update_SegR") || !SafeUpdate(spSegL, "Update_SegL")) break;

            CATLISTV(CATISpecObject_var) wireElems;
            wireElems.Append(spSegR);
            wireElems.Append(spInvR);
            wireElems.Append(spTipArc);
            wireElems.Append(spInvL);
            wireElems.Append(spSegL);
            CATIGSMAssemble_var spToothWire = spGSMFact->CreateAssemble(wireElems);
            CATIGSMProceduralView_var(spToothWire)->InsertInProceduralView(spWireSet);
            CATIAlias_var(spToothWire)->SetAlias(WideToCat(L"单齿封闭轮廓"));
            if (!SafeUpdate(spToothWire, "Update_ToothWire")) break;
            spToothWireObj = CATISpecObject_var(spToothWire);
        }

        {
            CATIGeometricalElement_var spWGeo(spToothWireObj);
            CATBody_var spWBody = (spWGeo != NULL_var) ? spWGeo->GetBodyResult() : NULL_var;
            if (spWBody != NULL_var) {
                CATLISTP(CATCell) wEdges, wVerts;
                spWBody->GetAllCells(wEdges, 1);
                spWBody->GetAllCells(wVerts, 0);
                printf("  ToothWire body: domains=%d, edges=%d, vertices=%d\n", spWBody->GetNbDomains(), wEdges.Size(), wVerts.Size());
            }
        }

        // Planar fill face for solid Pad
        CATIGSMFill_var spToothFill = spGSMFact->CreateFill();
        spToothFill->AddBound(spToothWireObj);
        CATIGSMProceduralView_var(spToothFill)->InsertInProceduralView(spWireSet);
        CATIAlias_var(spToothFill)->SetAlias(WideToCat(L"单齿填充面"));
        if (!SafeUpdate(spToothFill, "Update_ToothFill")) break;

        // Z-axis line for circular pattern (direction normal to XY plane = +Z)
        CATIGSMDirection_var spZDir = spGSMFact->CreateDirection(spXYPlane);
        CATIGSMLinePtDir_var spZAxis = spGSMFact->CreateLine(spCenterPt, spZDir, pZeroLen, pTenMm, CATGSMSameOrientation);
        CATIGSMProceduralView_var(spZAxis)->InsertInProceduralView(spWireSet);
        CATIAlias_var(spZAxis)->SetAlias(WideToCat(L"阵列旋转轴"));
        if (!SafeUpdate(spZAxis, "Update_ZAxis")) break;

        // 6. Build Solid in MainBody
        spPart->SetCurrentFeature(spMainBody);
        CATMathDirection dirZ(0.0, 0.0, 1.0);

        CATISpecObject_var spRootPadSpec = spPrtFact->CreatePad(spRootCircle);
        CATIPad_var spRootPad(spRootPadSpec);
        spRootPad->ModifyDirection(dirZ);
        spRootPad->ModifyEndType(catOffsetLimit);
        spRootPad->ModifyEndOffset(spec.b);
        spRootPad->ModifyStartType(catOffsetLimit);
        spRootPad->ModifyStartOffset(0.0);
        CATIAlias_var(spRootPadSpec)->SetAlias(WideToCat(L"凸台_齿根圆柱"));
        CATIPrtLimit_var spRootLimit(spRootPad->GetEndLimit());
        if (spRootLimit != NULL_var) {
            AddFormula2(spRelFact, spRelPub, "Formula_RootWidth", spRootLimit->GetOffsetParm(), pB, NULL_var, "a1");
        }
        if (!SafeUpdate(spRootPadSpec, "Update_RootPad")) break;

        CATISpecObject_var spToothPadSpec = spPrtFact->CreatePad(spToothFill);
        CATIPad_var spToothPad(spToothPadSpec);
        spToothPad->ModifyDirection(dirZ);
        spToothPad->ModifyEndType(catOffsetLimit);
        spToothPad->ModifyEndOffset(spec.b);
        spToothPad->ModifyStartType(catOffsetLimit);
        spToothPad->ModifyStartOffset(0.0);
        CATIAlias_var(spToothPadSpec)->SetAlias(WideToCat(L"凸台_单齿"));
        CATIPrtLimit_var spToothLimit(spToothPad->GetEndLimit());
        if (spToothLimit != NULL_var) {
            AddFormula2(spRelFact, spRelPub, "Formula_ToothWidth", spToothLimit->GetOffsetParm(), pB, NULL_var, "a1");
        }
        if (!SafeUpdate(spToothPadSpec, "Update_ToothPad")) break;

        CATISpecObject_var spPattSpec = spPrtFact->CreateCircPatt(
            spToothPadSpec, spZAxis, spCenterPt, TRUE,
            1, spec.z, 1.0, 360.0 / (double)spec.z, 1, 1, 0.0, TRUE, TRUE);
        CATIAlias_var(spPattSpec)->SetAlias(WideToCat(L"轮齿圆形阵列"));
        CATICircPatt_var spPatt(spPattSpec);
        if (spPatt != NULL_var) {
            AddFormula2(spRelFact, spRelPub, "Formula_PattCount", spPatt->GetParmNba(), pZ, NULL_var, "a1");
            AddFormula2(spRelFact, spRelPub, "Formula_PattStep", spPatt->GetParmStepa(), pAngStep, NULL_var, "a1");
        }
        if (!SafeUpdate(spPattSpec, "Update_CircPattern")) break;
        if (spPatt != NULL_var) {
            CATMathPoint cpt = spPatt->GetCenterPoint();
            CATMathLine axl = spPatt->GetAxisLine();
            CATMathDirection adir; axl.GetDirection(adir);
            printf("  CircPatt: center=(%.3f,%.3f,%.3f) dir=(%.3f,%.3f,%.3f) nba=%d stepa=%.4f instRot=%d\n",
                   cpt.GetX(), cpt.GetY(), cpt.GetZ(), adir.GetX(), adir.GetY(), adir.GetZ(),
                   spPatt->GetNba(), spPatt->GetStepa(), (int)spPatt->GetInstRot());
        }

        // Hide construction wireframe set
        CATIVisProperties_var spVis(spWireSet);
        if (spVis != NULL_var) {
            CATVisPropertiesValues visVals;
            visVals.SetShow(CATNoShowAttr);
            spVis->SetPropertiesAtt(visVals, CATVPShow, CATVPGlobalType);
        }

        CATISpecObject_var spPartSpec(spPart);
        if (!SafeUpdate(spPartSpec, "Update_FullPart")) break;

        if (!VerifyMainBodySolid(spPart, spec.da * 0.5, spec.b)) break;

        outMainBody = spMainBody;
        if (saveAndClose) {
            hr = CATDocumentServices::SaveAs(*pDoc, uOutPath);
            if (FAILED(hr)) {
                printf("[ERROR] SaveAs failed: hr=0x%08X\n", (unsigned int)hr);
                break;
            }
            printf("  Saved successfully: %s\n", WideToUtf8(workPath).c_str());
        }
        ok = true;
    } while (false);

    if (!ok || saveAndClose) {
        piPrtCont->Release();
        CATDocumentServices::Remove(*pDoc);
    } else {
        outDoc = pDoc;
        outCont = piPrtCont;
    }
    return ok;
}

static bool VerifyMainBodySolid(const CATIPrtPart_var& spPart, double expectedRa, double expectedB) {
    CATIPartRequest_var spPartReq(spPart);
    CATBaseUnknown_var spMainBodyBU;
    if (spPartReq != NULL_var) spPartReq->GetMainBody("MfDefault3DView", spMainBodyBU);
    CATIBodyRequest_var spBodyReq(spMainBodyBU);
    if (spBodyReq == NULL_var) {
        printf("  [ERROR] Missing CATIBodyRequest on MainBody.\n");
        return false;
    }
    CATLISTV(CATBaseUnknown_var) resList;
    if (FAILED(spBodyReq->GetResults("", resList)) || resList.Size() < 1) {
        printf("  [ERROR] MainBody has no solid results.\n");
        return false;
    }
    CATIGeometricalElement_var spBodyGeo(resList[1]);
    CATBody_var spBody = (spBodyGeo != NULL_var) ? spBodyGeo->GetBodyResult() : NULL_var;
    if (spBody == NULL_var) {
        printf("  [ERROR] MainBody GetBodyResult returned NULL.\n");
        return false;
    }

    double vol = 0.0;
    CATSoftwareConfiguration* pCfg = new CATSoftwareConfiguration();
    CATTopData topData(pCfg);
    CATICGMDynMassProperties3D* pMass = CATCGMDynCreateMassProperties3D(&topData, spBody);
    if (pMass) {
        vol = pMass->GetVolume();
        pMass->Release();
    }
    pCfg->Release();

    CATMathBox box = spBody->GetBoundingBox();
    double xmin, xmax, ymin, ymax, zmin, zmax;
    box.GetLimits(xmin, xmax, ymin, ymax, zmin, zmax);
    printf("  [VERIFIED SOLID] Vol=%.2f mm^3, BBox X[%.3f, %.3f] Y[%.3f, %.3f] Z[%.3f, %.3f]\n",
           vol, xmin, xmax, ymin, ymax, zmin, zmax);

    bool symY  = fabs(ymin + ymax) < 0.05;
    bool raOk  = (xmax >= expectedRa - 0.05 && xmax <= expectedRa + 0.50);
    bool negOk = (xmin <= -(expectedRa * 0.85) && xmin >= -expectedRa - 0.50);
    bool bOk   = (zmin >= -0.25 && zmin <= 0.1 && zmax >= expectedB - 0.1 && zmax <= expectedB + 0.25);
    if (vol <= 0.0 || !symY || !raOk || !negOk || !bOk) {
        printf("  [ERROR] Solid validation failed! (expected ra=%.3f, b=%.3f)\n", expectedRa, expectedB);
        return false;
    }
    return true;
}

static bool VerifyParametricZEdit(const wchar_t* partPath, int newZ) {
    printf("--- Verifying Parametric Reopen & Edit z=%d ---\n", newZ);
    CATDocument* pDoc = NULL;
    CATUnicodeString uPath = WideToCat(partPath);
    if (FAILED(CATDocumentServices::OpenDocument(uPath, pDoc)) || !pDoc) {
        printf("  [ERROR] Failed to open part for VerifyZ.\n");
        return false;
    }
    bool ok = false;
    CATInit_var spInit(pDoc);
    CATIPrtContainer* piPrtCont = (spInit != NULL_var) ? (CATIPrtContainer*)spInit->GetRootContainer("CATIPrtContainer") : NULL;
    if (piPrtCont) {
        CATIPrtPart_var spPart = piPrtCont->GetPart();
        CATIParmPublisher_var spPartPub(spPart);
        CATICkeParm_var pZ, pDa, pB;
        if (spPartPub != NULL_var) {
            CATLISTV(CATISpecObject_var) parms;
            spPartPub->GetAllChildren("CATICkeParm", parms);
            CATUnicodeString uZ = WideToCat(L"齿数z"), uDa = WideToCat(L"齿顶圆直径da"), uB = WideToCat(L"齿宽b");
            for (int i = 1; i <= parms.Size(); ++i) {
                CATICkeParm_var p(parms[i]);
                if (p == NULL_var) continue;
                CATIAlias_var al(p);
                CATUnicodeString nm = (al != NULL_var) ? al->GetAlias() : CATUnicodeString("");
                CATUnicodeString ir = p->InternalRole();
                if (nm == uZ || ir == uZ) pZ = p;
                else if (nm == uDa || ir == uDa) pDa = p;
                else if (nm == uB || ir == uB) pB = p;
            }
            printf("  Found %d parameters in part.\n", parms.Size());
        }
        if (pZ != NULL_var && pDa != NULL_var && pB != NULL_var) {
            pZ->Valuate(newZ);
            CATISpecObject_var spPartSpec(spPart);
            if (SafeUpdate(spPartSpec, "VerifyParametricUpdate")) {
                double raMm = pDa->Value()->AsReal() * 1000.0 * 0.5;
                double bMm  = pB->Value()->AsReal() * 1000.0;
                printf("  Updated formula da=%.3f mm (ra=%.3f mm), b=%.3f mm\n", raMm * 2.0, raMm, bMm);
                ok = VerifyMainBodySolid(spPart, raMm, bMm);
            }
        } else {
            printf("  [ERROR] Could not locate z/da/b parameters in reopened part.\n");
        }
        piPrtCont->Release();
    }
    CATDocumentServices::Remove(*pDoc);
    return ok;
}

static void ResolvePlacement(const AssemblyPlacement& req, double aw,
                             double p1Out[3], double p2Out[3],
                             double u[3], double v[3], double w[3]) {
    char ax = (char)toupper((unsigned char)req.axis);
    if (ax != 'X' && ax != 'Y' && ax != 'Z') ax = 'Y';

    double u0[3] = {1.0, 0.0, 0.0};
    if (ax == 'X') {
        w[0] = 1.0; w[1] = 0.0; w[2] = 0.0;
        u0[0] = 0.0; u0[1] = 1.0; u0[2] = 0.0;
    } else if (ax == 'Z') {
        w[0] = 0.0; w[1] = 0.0; w[2] = 1.0;
        u0[0] = 1.0; u0[1] = 0.0; u0[2] = 0.0;
    } else {
        w[0] = 0.0; w[1] = 1.0; w[2] = 0.0;
        u0[0] = 1.0; u0[1] = 0.0; u0[2] = 0.0;
    }

    for (int k = 0; k < 3; ++k) p1Out[k] = req.p1[k];

    if (!req.useP2) {
        for (int k = 0; k < 3; ++k) {
            u[k] = u0[k];
            p2Out[k] = p1Out[k] + aw * u[k];
        }
        printf("  Placement: axis=%c, P1=(%.3f, %.3f, %.3f), auto P2=(%.3f, %.3f, %.3f)\n",
               ax, p1Out[0], p1Out[1], p1Out[2], p2Out[0], p2Out[1], p2Out[2]);
    } else {
        double d[3] = { req.p2[0] - req.p1[0], req.p2[1] - req.p1[1], req.p2[2] - req.p1[2] };
        double dAxial = d[0] * w[0] + d[1] * w[1] + d[2] * w[2];
        double dPerp[3] = { d[0] - dAxial * w[0], d[1] - dAxial * w[1], d[2] - dAxial * w[2] };
        double lPerp = sqrt(dPerp[0] * dPerp[0] + dPerp[1] * dPerp[1] + dPerp[2] * dPerp[2]);
        if (lPerp < 1e-6) {
            for (int k = 0; k < 3; ++k) {
                u[k] = u0[k];
                p2Out[k] = p1Out[k] + aw * u[k];
            }
            printf("  Placement: axis=%c, P2 overlaps P1 along radial plane -> fallback P2=(%.3f, %.3f, %.3f)\n",
                   ax, p2Out[0], p2Out[1], p2Out[2]);
        } else {
            for (int k = 0; k < 3; ++k) u[k] = dPerp[k] / lPerp;
            if (fabs(lPerp - aw) <= 0.01) {
                for (int k = 0; k < 3; ++k) p2Out[k] = req.p2[k];
                printf("  Placement: axis=%c, P1=(%.3f, %.3f, %.3f), accepted input P2=(%.3f, %.3f, %.3f) (L_perp=%.4f ~ aw=%.4f)\n",
                       ax, p1Out[0], p1Out[1], p1Out[2], p2Out[0], p2Out[1], p2Out[2], lPerp, aw);
            } else {
                for (int k = 0; k < 3; ++k) p2Out[k] = p1Out[k] + aw * u[k];
                printf("  Placement: axis=%c, P1=(%.3f, %.3f, %.3f), P2 L_perp=%.4f != aw=%.4f -> using P1->P2 dir=(%.4f,%.4f,%.4f), P2=(%.3f, %.3f, %.3f)\n",
                       ax, p1Out[0], p1Out[1], p1Out[2], lPerp, aw, u[0], u[1], u[2], p2Out[0], p2Out[1], p2Out[2]);
            }
        }
    }

    // Right-handed basis: v = w x u so u x v = w
    v[0] = w[1] * u[2] - w[2] * u[1];
    v[1] = w[2] * u[0] - w[0] * u[2];
    v[2] = w[0] * u[1] - w[1] * u[0];
}

static bool CreateMeshedProduct(const wchar_t* g1Path, const wchar_t* g2Path, const wchar_t* prodPath,
                                const GearPairParams& gp, const AssemblyPlacement& place) {
    printf("--- Assembling Meshed Product (aw=%.4f mm, phi2=%.4f deg) ---\n",
           gp.workCenterDist, (CATPI + CATPI / gp.z2) * 180.0 / CATPI);

    CATDocument* pProdDoc = NULL;
    HRESULT hr = CATDocumentServices::New("Product", pProdDoc);
    if (FAILED(hr) || !pProdDoc) {
        printf("[ERROR] Failed to create CATProduct: hr=0x%08X\n", (unsigned int)hr);
        return false;
    }

    CATIDocRoots* piRoots = NULL;
    hr = pProdDoc->QueryInterface(IID_CATIDocRoots, (void**)&piRoots);
    if (FAILED(hr) || !piRoots) { CATDocumentServices::Remove(*pProdDoc); return false; }
    CATListValCATBaseUnknown_var* pRootList = piRoots->GiveDocRoots();
    CATIProduct_var spRootProd = (pRootList && pRootList->Size() > 0) ? CATIProduct_var((*pRootList)[1]) : NULL_var;
    if (pRootList) delete pRootList;
    piRoots->Release();
    if (spRootProd == NULL_var) { CATDocumentServices::Remove(*pProdDoc); return false; }

    spRootProd->SetPartNumber("GearPair_Meshed_Assembly");

    CATDocument* pDoc1 = NULL;
    CATDocument* pDoc2 = NULL;
    CATDocumentServices::OpenDocument(WideToCat(g1Path), pDoc1);
    CATDocumentServices::OpenDocument(WideToCat(g2Path), pDoc2);
    if (!pDoc1 || !pDoc2) {
        printf("[ERROR] Failed to reopen gear CATParts for product assembly.\n");
        if (pDoc1) CATDocumentServices::Remove(*pDoc1);
        if (pDoc2) CATDocumentServices::Remove(*pDoc2);
        CATDocumentServices::Remove(*pProdDoc);
        return false;
    }

    auto getRefProd = [](CATDocument* doc) -> CATIProduct_var {
        CATIDocRoots* r = NULL;
        if (FAILED(doc->QueryInterface(IID_CATIDocRoots, (void**)&r)) || !r) return NULL_var;
        CATListValCATBaseUnknown_var* lst = r->GiveDocRoots();
        CATIProduct_var res = (lst && lst->Size() > 0) ? CATIProduct_var((*lst)[1]) : NULL_var;
        if (lst) delete lst;
        r->Release();
        return res;
    };

    CATIProduct_var spRef1 = getRefProd(pDoc1);
    CATIProduct_var spRef2 = getRefProd(pDoc2);
    CATIProduct_var spInst1 = spRootProd->AddProduct(spRef1);
    CATIProduct_var spInst2 = spRootProd->AddProduct(spRef2);
    if (spInst1 != NULL_var) spInst1->SetPrdInstanceName("Gear1_Pinion");
    if (spInst2 != NULL_var) spInst2->SetPrdInstanceName("Gear2_Wheel");

    double p1[3], p2[3], u[3], v[3], w[3];
    ResolvePlacement(place, gp.workCenterDist, p1, p2, u, v, w);

    // Position Gear1 at p1 with local +X -> u, +Y -> v, +Z -> w
    CATIMovable_var spMov1(spInst1);
    if (spMov1 != NULL_var) {
        double mat1[12] = {
            u[0], u[1], u[2],
            v[0], v[1], v[2],
            w[0], w[1], w[2],
            p1[0], p1[1], p1[2]
        };
        CATMathTransformation tr1(mat1);
        spMov1->SetPosition(tr1, NULL_var);
    }

    // Position Gear2 at p2 rotated around w by (PI + PI/z2)
    CATIMovable_var spMov2(spInst2);
    if (spMov2 != NULL_var) {
        double phi2 = CATPI + CATPI / (double)gp.z2;
        double c2 = cos(phi2);
        double s2 = sin(phi2);
        double u2[3] = { c2 * u[0] + s2 * v[0], c2 * u[1] + s2 * v[1], c2 * u[2] + s2 * v[2] };
        double v2[3] = { -s2 * u[0] + c2 * v[0], -s2 * u[1] + c2 * v[1], -s2 * u[2] + c2 * v[2] };
        double mat2[12] = {
            u2[0], u2[1], u2[2],
            v2[0], v2[1], v2[2],
            w[0],  w[1],  w[2],
            p2[0], p2[1], p2[2]
        };
        CATMathTransformation tr2(mat2);
        spMov2->SetPosition(tr2, NULL_var);
    }

    if (spMov1 != NULL_var && spMov2 != NULL_var) {
        CATMathTransformation a1, a2;
        double c1[12] = {0}, c2[12] = {0};
        if (SUCCEEDED(spMov1->GetAbsPosition(a1)) && SUCCEEDED(spMov2->GetAbsPosition(a2))) {
            a1.GetCoefficients(c1, 12);
            a2.GetCoefficients(c2, 12);
            double dx = c2[9] - c1[9], dy = c2[10] - c1[10], dz = c2[11] - c1[11];
            double axProj = dx * c1[6] + dy * c1[7] + dz * c1[8];
            double rp = sqrt((dx - axProj * c1[6]) * (dx - axProj * c1[6]) +
                             (dy - axProj * c1[7]) * (dy - axProj * c1[7]) +
                             (dz - axProj * c1[8]) * (dz - axProj * c1[8]));
            printf("  [VERIFIED ASSEMBLY] G1=(%.3f,%.3f,%.3f) G2=(%.3f,%.3f,%.3f) Axis=(%.1f,%.1f,%.1f) RadialDist=%.4f mm\n",
                   c1[9], c1[10], c1[11], c2[9], c2[10], c2[11], c1[6], c1[7], c1[8], rp);
        }
    }

    hr = CATDocumentServices::SaveAs(*pProdDoc, WideToCat(prodPath));
    bool ok = SUCCEEDED(hr);
    if (ok) printf("  Product saved successfully.\n");
    else printf("[ERROR] Product SaveAs failed: hr=0x%08X\n", (unsigned int)hr);

    CATDocumentServices::Remove(*pDoc2);
    CATDocumentServices::Remove(*pDoc1);
    CATDocumentServices::Remove(*pProdDoc);
    return ok;
}

static void EnsureDirTreeW(const std::wstring& dirPath) {
    if (dirPath.empty()) return;
    for (size_t i = 0; i < dirPath.size(); ++i) {
        if (dirPath[i] == L'\\' || dirPath[i] == L'/') {
            if (i > 2) {
                std::wstring sub = dirPath.substr(0, i);
                CreateDirectoryW(sub.c_str(), NULL);
            }
        }
    }
    CreateDirectoryW(dirPath.c_str(), NULL);
}

static bool LoadUtf8JobFile(const wchar_t* jobFile, std::wstring& seedPath, std::wstring& outDir,
                            double& m, int& z1, int& z2, double& b1, double& b2,
                            double& alphaDeg, double& x1, double& x2, double& backlash, double& filletR,
                            AssemblyPlacement& place) {
    FILE* fp = _wfopen(jobFile, L"rb");
    if (!fp) return false;
    char buf[1024];
    bool first = true;
    while (fgets(buf, sizeof(buf), fp)) {
        std::string line(buf);
        if (first) {
            first = false;
            if (line.size() >= 3 && (unsigned char)line[0] == 0xEF && (unsigned char)line[1] == 0xBB && (unsigned char)line[2] == 0xBF) {
                line.erase(0, 3);
            }
        }
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' ')) line.pop_back();
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = line.substr(0, eq);
        std::string v = line.substr(eq + 1);
        if (k == "seed") seedPath = BytesToWide(v.c_str(), CP_UTF8);
        else if (k == "outDir") outDir = BytesToWide(v.c_str(), CP_UTF8);
        else if (k == "m") m = atof(v.c_str());
        else if (k == "z1") z1 = atoi(v.c_str());
        else if (k == "z2") z2 = atoi(v.c_str());
        else if (k == "b1") b1 = atof(v.c_str());
        else if (k == "b2") b2 = atof(v.c_str());
        else if (k == "alpha") alphaDeg = atof(v.c_str());
        else if (k == "x1") x1 = atof(v.c_str());
        else if (k == "x2") x2 = atof(v.c_str());
        else if (k == "backlash") backlash = atof(v.c_str());
        else if (k == "filletR") filletR = atof(v.c_str());
        else if (k == "axis" && !v.empty()) place.axis = (char)toupper((unsigned char)v[0]);
        else if (k == "cx1") place.p1[0] = atof(v.c_str());
        else if (k == "cy1") place.p1[1] = atof(v.c_str());
        else if (k == "cz1") place.p1[2] = atof(v.c_str());
        else if (k == "useP2") place.useP2 = (atoi(v.c_str()) != 0);
        else if (k == "cx2") place.p2[0] = atof(v.c_str());
        else if (k == "cy2") place.p2[1] = atof(v.c_str());
        else if (k == "cz2") place.p2[2] = atof(v.c_str());
    }
    fclose(fp);
    return true;
}

static bool VerifyTransformedBody(const CATISpecObject_var& spBody, const char* tag) {
    CATIBodyRequest_var spBodyReq(spBody);
    if (spBodyReq == NULL_var) return false;
    CATLISTV(CATBaseUnknown_var) resList;
    if (FAILED(spBodyReq->GetResults("", resList)) || resList.Size() < 1) return false;
    CATIGeometricalElement_var spGeo(resList[1]);
    CATBody_var spB = (spGeo != NULL_var) ? spGeo->GetBodyResult() : NULL_var;
    if (spB == NULL_var) return false;
    double vol = 0.0;
    CATSoftwareConfiguration* pCfg = new CATSoftwareConfiguration();
    CATTopData topData(pCfg);
    CATICGMDynMassProperties3D* pMass = CATCGMDynCreateMassProperties3D(&topData, spB);
    if (pMass) { vol = pMass->GetVolume(); pMass->Release(); }
    pCfg->Release();
    CATMathBox box = spB->GetBoundingBox();
    double xmin, xmax, ymin, ymax, zmin, zmax;
    box.GetLimits(xmin, xmax, ymin, ymax, zmin, zmax);
    printf("  [VERIFIED %s] Vol=%.2f mm^3, BBox X[%.3f, %.3f] Y[%.3f, %.3f] Z[%.3f, %.3f]\n",
           tag, vol, xmin, xmax, ymin, ymax, zmin, zmax);
    return vol > 0.0;
}

static bool BuildSinglePartGearPair(const wchar_t* seedPath, const wchar_t* outPartPath,
                                    const GearPairParams& gp, const AssemblyPlacement& place) {
    wchar_t tmpDir[MAX_PATH] = {0}, tmpGear2[MAX_PATH] = {0};
    GetTempPathW(MAX_PATH, tmpDir);
    swprintf_s(tmpGear2, L"%sGear2_tmp_%lu.CATPart", tmpDir, GetCurrentProcessId());

    CATDocument* pDoc1 = NULL;
    CATIPrtContainer* piCont1 = NULL;
    CATISpecObject_var spBody1;
    if (!BuildSingleGearDoc(seedPath, outPartPath, gp.g1, L"主动轮_Gear1", false, pDoc1, piCont1, spBody1)) {
        DeleteFileW(outPartPath);
        return false;
    }

    CATDocument* pDoc2 = NULL;
    CATIPrtContainer* piCont2 = NULL;
    CATISpecObject_var spBody2;
    if (!BuildSingleGearDoc(seedPath, tmpGear2, gp.g2, L"从动轮_Gear2", false, pDoc2, piCont2, spBody2)) {
        piCont1->Release();
        CATDocumentServices::Remove(*pDoc1);
        DeleteFileW(outPartPath);
        DeleteFileW(tmpGear2);
        return false;
    }

    bool ok = false;
    do {
        CATIPrtPart_var spPart1 = piCont1->GetPart();
        CATIPrtFactory_var spPrtFact1(piCont1);
        CATIMf3DAxisSystemFactory_var spAxisFact(piCont1);
        if (spPart1 == NULL_var || spPrtFact1 == NULL_var || spAxisFact == NULL_var) {
            printf("[ERROR] Missing Part/PrtFact/AxisSystemFactory.\n");
            break;
        }

        CATMmrInterPartCopy copier(spBody2, CATISpecObject_var(spPart1));
        copier.SetLinkMode(FALSE);
        copier.SetTypeOfCreatedSolidSet(FALSE);
        CATUnicodeString copyErr;
        HRESULT hr = copier.Run(&copyErr);
        CATISpecObject_var spCopiedBody2;
        if (FAILED(hr) || FAILED(copier.GetResult(spCopiedBody2)) || spCopiedBody2 == NULL_var) {
            printf("[ERROR] CATMmrInterPartCopy failed: hr=0x%08X %s\n", (unsigned int)hr, CatToUtf8(copyErr).c_str());
            break;
        }
        CATIAlias_var(spCopiedBody2)->SetAlias(WideToCat(L"从动轮_Gear2"));

        piCont2->Release(); piCont2 = NULL;
        CATDocumentServices::Remove(*pDoc2); pDoc2 = NULL;
        DeleteFileW(tmpGear2);

        double p1[3], p2[3], u[3], v[3], w[3];
        ResolvePlacement(place, gp.workCenterDist, p1, p2, u, v, w);
        double phi2 = CATPI + CATPI / (double)gp.z2;
        double c2 = cos(phi2), s2 = sin(phi2);
        double u2[3] = { c2 * u[0] + s2 * v[0], c2 * u[1] + s2 * v[1], c2 * u[2] + s2 * v[2] };
        double v2[3] = { -s2 * u[0] + c2 * v[0], -s2 * u[1] + c2 * v[1], -s2 * u[2] + c2 * v[2] };

        CATIMf3DAxisSystem_var spRefAxis, spAxis1, spAxis2;
        if (FAILED(spAxisFact->CreateAxisSystem(CATMathPoint(0.0, 0.0, 0.0), CATMathVector(1.0, 0.0, 0.0), CATMathVector(0.0, 1.0, 0.0), spRefAxis)) ||
            FAILED(spAxisFact->CreateAxisSystem(CATMathPoint(p1[0], p1[1], p1[2]), CATMathVector(u[0], u[1], u[2]), CATMathVector(v[0], v[1], v[2]), spAxis1)) ||
            FAILED(spAxisFact->CreateAxisSystem(CATMathPoint(p2[0], p2[1], p2[2]), CATMathVector(u2[0], u2[1], u2[2]), CATMathVector(v2[0], v2[1], v2[2]), spAxis2))) {
            printf("[ERROR] CreateAxisSystem failed.\n");
            break;
        }
        CATIAlias_var(spRefAxis)->SetAlias(WideToCat(L"基准坐标系"));
        CATIAlias_var(spAxis1)->SetAlias(WideToCat(L"主动轮坐标系"));
        CATIAlias_var(spAxis2)->SetAlias(WideToCat(L"从动轮坐标系"));

        CATISpecObject_var spAxes[3] = { spRefAxis, spAxis1, spAxis2 };
        for (int a = 0; a < 3; ++a) {
            SafeUpdate(spAxes[a], "Update_AxisSystem");
        }

        spPart1->SetCurrentFeature(spBody1);
        CATISpecObject_var spA2A1 = spPrtFact1->CreateAxisToAxis(spBody1, spRefAxis, spAxis1);
        CATIAlias_var(spA2A1)->SetAlias(WideToCat(L"主动轮空间定位"));
        if (!SafeUpdate(spA2A1, "Update_AxisToAxis_Gear1")) break;

        spPart1->SetCurrentFeature(spCopiedBody2);
        CATISpecObject_var spA2A2 = spPrtFact1->CreateAxisToAxis(spCopiedBody2, spRefAxis, spAxis2);
        CATIAlias_var(spA2A2)->SetAlias(WideToCat(L"从动轮空间定位"));
        if (!SafeUpdate(spA2A2, "Update_AxisToAxis_Gear2")) break;

        if (!SafeUpdate(CATISpecObject_var(spPart1), "Update_SinglePartPair")) break;

        // Hide everything except the two gear solid bodies (planes, geometrical sets, axis systems)
        auto setVisShow = [](const CATISpecObject_var& obj, CATShowAttribut attr) {
            CATIVisProperties_var v(obj);
            if (v != NULL_var) {
                CATVisPropertiesValues vv;
                vv.SetShow(attr);
                v->SetPropertiesAtt(vv, CATVPShow, CATVPGlobalType);
            }
        };
        CATLISTV(CATISpecObject_var) refPlanes = spPart1->GetReferencePlanes();
        for (int rp = 1; rp <= refPlanes.Size(); ++rp) setVisShow(refPlanes[rp], CATNoShowAttr);

        CATLISTV(CATISpecObject_var) gsmTools, axisSystems;
        CATIDescendants_var spPartDesc(spPart1);
        if (spPartDesc != NULL_var) {
            spPartDesc->GetAllChildren("CATIGSMTool", gsmTools);
            for (int g = 1; g <= gsmTools.Size(); ++g) setVisShow(gsmTools[g], CATNoShowAttr);
            spPartDesc->GetAllChildren("CATIMf3DAxisSystem", axisSystems);
            for (int a = 1; a <= axisSystems.Size(); ++a) setVisShow(axisSystems[a], CATNoShowAttr);
        }
        for (int a = 0; a < 3; ++a) setVisShow(spAxes[a], CATNoShowAttr);
        setVisShow(spBody1, CATShowAttr);
        setVisShow(spCopiedBody2, CATShowAttr);

        // Create visible meshing reference geometry (pitch circles, centers, shaft axes, center-to-center line)
        CATIGSMFactory_var spGSMFact1(piCont1);
        CATIMechanicalRootFactory_var spRootFact(piCont1);
        CATISpecObject_var spParmSet1 = CATCkeGlobalFunctions::GetFunctionFactory()->GetCurrentSet(
            CATICkeFunctionFactory::Parameter, CATIParmPublisher_var(spPart1), CATCke::True);
        CATICkeParmFactory_var spParmFact1 = (spParmSet1 != NULL_var) ? spParmSet1->GetFeatContainer() : NULL_var;
        CATISpecObject_var spRefSet;
        if (spGSMFact1 != NULL_var && spRootFact != NULL_var && spParmFact1 != NULL_var &&
            SUCCEEDED(spRootFact->CreateGeometricalSet(WideToCat(L"啮合基准特征"), CATISpecObject_var(spPart1), spRefSet)) &&
            spRefSet != NULL_var) {
            setVisShow(spRefSet, CATShowAttr);

            auto styleLine = [](const CATISpecObject_var& obj, unsigned int r, unsigned int g, unsigned int b, unsigned int ltype, unsigned int wpx) {
                CATIVisProperties_var vp(obj);
                if (vp == NULL_var) return;
                CATVisPropertiesValues vc, vt, vw;
                vc.SetColor(r, g, b); vp->SetPropertiesAtt(vc, CATVPColor, CATVPLine);
                vt.SetLineType(ltype); vp->SetPropertiesAtt(vt, CATVPLineType, CATVPLine);
                vw.SetWidth(wpx); vp->SetPropertiesAtt(vw, CATVPWidth, CATVPLine);
            };
            auto stylePoint = [](const CATISpecObject_var& obj, unsigned int r, unsigned int g, unsigned int b) {
                CATIVisProperties_var vp(obj);
                if (vp == NULL_var) return;
                CATVisPropertiesValues vc;
                vc.SetColor(r, g, b); vp->SetPropertiesAtt(vc, CATVPColor, CATVPPoint);
            };
            auto makePt = [&](const double pt[3], const wchar_t* alias, bool visible) -> CATISpecObject_var {
                CATICkeParm_var px = spParmFact1->CreateLength("", pt[0] / 1000.0);
                CATICkeParm_var py = spParmFact1->CreateLength("", pt[1] / 1000.0);
                CATICkeParm_var pz = spParmFact1->CreateLength("", pt[2] / 1000.0);
                CATIGSMPointCoord_var spPt = spGSMFact1->CreatePoint(px, py, pz);
                CATIGSMProceduralView_var(spPt)->InsertInProceduralView(spRefSet);
                if (alias && alias[0]) CATIAlias_var(spPt)->SetAlias(WideToCat(alias));
                SafeUpdate(spPt, "Update_RefPt");
                setVisShow(spPt, visible ? CATShowAttr : CATNoShowAttr);
                if (visible) stylePoint(spPt, 255, 220, 0);
                return CATISpecObject_var(spPt);
            };
            auto printWireBBox = [](const char* label, const CATISpecObject_var& obj) {
                CATIGeometricalElement_var g(obj);
                CATBody_var b = (g != NULL_var) ? g->GetBodyResult() : NULL_var;
                if (b == NULL_var) return;
                CATMathBox box = b->GetBoundingBox();
                double x0, x1, y0, y1, z0, z1;
                box.GetLimits(x0, x1, y0, y1, z0, z1);
                printf("  [VERIFIED REF %s] BBox X[%.3f, %.3f] Y[%.3f, %.3f] Z[%.3f, %.3f]\n",
                       label, x0, x1, y0, y1, z0, z1);
            };

            double rw1 = gp.workCenterDist * (double)gp.z1 / (double)(gp.z1 + gp.z2);
            double rw2 = gp.workCenterDist * (double)gp.z2 / (double)(gp.z1 + gp.z2);
            double pPitch[3] = { p1[0] + rw1 * u[0], p1[1] + rw1 * u[1], p1[2] + rw1 * u[2] };

            CATISpecObject_var spC1 = makePt(p1, L"主动轮圆心", true);
            CATISpecObject_var spC2 = makePt(p2, L"从动轮圆心", true);
            makePt(pPitch, L"啮合节点", true);

            double ext1 = (0.35 * gp.b1 > 3.0) ? 0.35 * gp.b1 : 3.0;
            double ext2 = (0.35 * gp.b2 > 3.0) ? 0.35 * gp.b2 : 3.0;
            double a1A[3] = { p1[0] - ext1 * w[0], p1[1] - ext1 * w[1], p1[2] - ext1 * w[2] };
            double a1B[3] = { p1[0] + (gp.b1 + ext1) * w[0], p1[1] + (gp.b1 + ext1) * w[1], p1[2] + (gp.b1 + ext1) * w[2] };
            double a2A[3] = { p2[0] - ext2 * w[0], p2[1] - ext2 * w[1], p2[2] - ext2 * w[2] };
            double a2B[3] = { p2[0] + (gp.b2 + ext2) * w[0], p2[1] + (gp.b2 + ext2) * w[1], p2[2] + (gp.b2 + ext2) * w[2] };

            CATIGSMLinePtPt_var spShaft1 = spGSMFact1->CreateLine(makePt(a1A, L"轴线端点1A", false), makePt(a1B, L"轴线端点1B", false));
            CATIGSMProceduralView_var(spShaft1)->InsertInProceduralView(spRefSet);
            CATIAlias_var(spShaft1)->SetAlias(WideToCat(L"主动轮轴线"));
            SafeUpdate(spShaft1, "Update_Shaft1");
            setVisShow(spShaft1, CATShowAttr);
            styleLine(spShaft1, 0, 140, 255, 4, 2);

            CATIGSMLinePtPt_var spShaft2 = spGSMFact1->CreateLine(makePt(a2A, L"轴线端点2A", false), makePt(a2B, L"轴线端点2B", false));
            CATIGSMProceduralView_var(spShaft2)->InsertInProceduralView(spRefSet);
            CATIAlias_var(spShaft2)->SetAlias(WideToCat(L"从动轮轴线"));
            SafeUpdate(spShaft2, "Update_Shaft2");
            setVisShow(spShaft2, CATShowAttr);
            styleLine(spShaft2, 0, 140, 255, 4, 2);

            CATIGSMLinePtPt_var spCenterLine = spGSMFact1->CreateLine(spC1, spC2);
            CATIGSMProceduralView_var(spCenterLine)->InsertInProceduralView(spRefSet);
            CATIAlias_var(spCenterLine)->SetAlias(WideToCat(L"两轮中心连线"));
            SafeUpdate(spCenterLine, "Update_CenterLine");
            setVisShow(spCenterLine, CATShowAttr);
            styleLine(spCenterLine, 0, 180, 80, 3, 2);

            char axChar = (char)toupper((unsigned char)place.axis);
            CATISpecObject_var spPitchSupport = (axChar == 'X') ? refPlanes[2] : ((axChar == 'Z') ? refPlanes[1] : refPlanes[3]);
            CATICkeParm_var pRw1 = spParmFact1->CreateLength("", rw1 / 1000.0);
            CATICkeParm_var pRw2 = spParmFact1->CreateLength("", rw2 / 1000.0);

            CATIGSMCircleCtrRad_var spPitchC1 = spGSMFact1->CreateCircle(spC1, spPitchSupport, FALSE, pRw1, NULL_var, NULL_var);
            CATIGSMCircle_var(spPitchC1)->SetLimitation(CATIGSMCircle::Whole);
            CATIGSMProceduralView_var(spPitchC1)->InsertInProceduralView(spRefSet);
            CATIAlias_var(spPitchC1)->SetAlias(WideToCat(L"主动轮节圆"));
            SafeUpdate(spPitchC1, "Update_PitchCircle1");
            setVisShow(spPitchC1, CATShowAttr);
            styleLine(spPitchC1, 255, 210, 0, 4, 2);

            CATIGSMCircleCtrRad_var spPitchC2 = spGSMFact1->CreateCircle(spC2, spPitchSupport, FALSE, pRw2, NULL_var, NULL_var);
            CATIGSMCircle_var(spPitchC2)->SetLimitation(CATIGSMCircle::Whole);
            CATIGSMProceduralView_var(spPitchC2)->InsertInProceduralView(spRefSet);
            CATIAlias_var(spPitchC2)->SetAlias(WideToCat(L"从动轮节圆"));
            SafeUpdate(spPitchC2, "Update_PitchCircle2");
            setVisShow(spPitchC2, CATShowAttr);
            styleLine(spPitchC2, 255, 210, 0, 4, 2);

            printf("  [VERIFIED REF Pitch] rw1=%.4f mm, rw2=%.4f mm, rw1+rw2=%.4f mm (aw=%.4f mm)\n",
                   rw1, rw2, rw1 + rw2, gp.workCenterDist);
            printWireBBox("PitchC1", spPitchC1);
            printWireBBox("PitchC2", spPitchC2);
            printWireBBox("CenterLine", spCenterLine);
            printWireBBox("Shaft1", spShaft1);
            printWireBBox("Shaft2", spShaft2);
        }

        if (!SafeUpdate(CATISpecObject_var(spPart1), "Update_SinglePartPair_Final")) break;

        if (!VerifyTransformedBody(spBody1, "SINGLE_PART_GEAR1") ||
            !VerifyTransformedBody(spCopiedBody2, "SINGLE_PART_GEAR2")) break;

        hr = CATDocumentServices::SaveAs(*pDoc1, WideToCat(outPartPath));
        if (FAILED(hr)) {
            printf("[ERROR] SaveAs single CATPart failed: hr=0x%08X\n", (unsigned int)hr);
            break;
        }
        printf("  Single-CATPart GearPair saved successfully: %s\n", WideToUtf8(outPartPath).c_str());
        ok = true;
    } while (false);

    if (piCont2) piCont2->Release();
    if (pDoc2) CATDocumentServices::Remove(*pDoc2);
    DeleteFileW(tmpGear2);
    piCont1->Release();
    CATDocumentServices::Remove(*pDoc1);
    return ok;
}

int main(int argc, char** argv) {
    SetConsoleOutputCP(65001);

    if (argc >= 4 && strcmp(argv[1], "--verify-z") == 0) {
        std::wstring pW = BytesToWide(argv[2], CP_ACP);
        CATSession* pSession = NULL;
        char sessName[] = "GearVerifySession";
        if (FAILED(::Create_Session(sessName, pSession)) || !pSession) return 2;
        bool ok = true;
        for (int i = 3; i < argc; ++i) {
            ok = ok && VerifyParametricZEdit(pW.c_str(), atoi(argv[i]));
        }
        ::Delete_Session(sessName);
        printf("=== VerifyZ finished: %s ===\n", ok ? "SUCCESS" : "FAILED");
        return ok ? 0 : 4;
    }

    std::wstring seedPath = L"c:\\齿轮生成\\CAA_GearPair\\win_b64\\code\\bin\\GearSeed.CATPart";
    std::wstring outDir   = L"c:\\齿轮生成\\Output_CAA";
    double m = 1.0, b1 = 6.0, b2 = 6.0, alphaDeg = 20.0, x1 = 0.0, x2 = 0.0, backlash = 0.0, filletR = 0.1;
    int z1 = 34, z2 = 60;
    AssemblyPlacement place;

    wchar_t jobEnv[MAX_PATH] = {0};
    if (GetEnvironmentVariableW(L"GEAR_JOB_FILE", jobEnv, MAX_PATH) > 0) {
        LoadUtf8JobFile(jobEnv, seedPath, outDir, m, z1, z2, b1, b2, alphaDeg, x1, x2, backlash, filletR, place);
    } else if (argc >= 10) {
        seedPath = BytesToWide(argv[1], CP_ACP);
        outDir   = BytesToWide(argv[2], CP_ACP);
        m        = atof(argv[3]);
        z1       = atoi(argv[4]);
        z2       = atoi(argv[5]);
        b1       = atof(argv[6]);
        b2       = atof(argv[7]);
        alphaDeg = atof(argv[8]);
        x1       = atof(argv[9]);
        x2       = (argc >= 11) ? atof(argv[10]) : 0.0;
        backlash = (argc >= 12) ? atof(argv[11]) : 0.0;
        filletR  = (argc >= 13) ? atof(argv[12]) : 0.1;
    }

    GearPairParams gp;
    if (!ComputeGearPair(m, z1, z2, b1, b2, alphaDeg, x1, x2, backlash, filletR, gp)) {
        return 1;
    }

    printf("GearPair Math: m=%.3f, z1=%d, z2=%d, alpha=%.2f deg, x1=%.3f, x2=%.3f, R=%.3f\n", m, z1, z2, alphaDeg, x1, x2, filletR);
    printf("  Working center distance aw = %.6f mm (std=%.6f mm), alpha_w=%.4f deg, eps=%.3f\n",
           gp.workCenterDist, gp.stdCenterDist, gp.workAlphaRad * 180.0 / CATPI, gp.contactRatio);

    EnsureDirTreeW(outDir);

    CATSession* pSession = NULL;
    char sessName[] = "GearPairCAASession";
    HRESULT hr = ::Create_Session(sessName, pSession);
    if (FAILED(hr) || !pSession) {
        printf("[ERROR] Create_Session failed: hr=0x%08X\n", (unsigned int)hr);
        return 2;
    }

    wchar_t wPair[MAX_PATH];
    swprintf_s(wPair, L"%s\\GearPair_z%d_z%d.CATPart", outDir.c_str(), z1, z2);

    bool ok3 = BuildSinglePartGearPair(seedPath.c_str(), wPair, gp, place);

    ::Delete_Session(sessName);
    printf("=== GearPairWorker finished: %s ===\n", ok3 ? "SUCCESS" : "FAILED");
    return ok3 ? 0 : 3;
}
