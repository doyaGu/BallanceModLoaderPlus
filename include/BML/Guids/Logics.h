#ifndef BML_GUIDS_LOGICS_H
#define BML_GUIDS_LOGICS_H

#include "CKTypes.h"

//
// Logics
//

// Building Blocks
#define VT_LOGICS_ADDROW CKGUID(0x1c7e5dc6, 0x3f6423c2)
#define VT_LOGICS_CHANGEVALUEIF CKGUID(0x7253edb, 0x4d1237ed)
#define VT_LOGICS_CLEARARRAY CKGUID(0x35c9352f, 0x7b1a193b)
#define VT_LOGICS_COLUMNPRODUCT CKGUID(0x39f64a9a, 0x682b721c)
#define VT_LOGICS_COLUMNSUM CKGUID(0x49124252, 0x1be909a4)
#define VT_LOGICS_COLUMNTRANSFORM CKGUID(0x4f8f0dfe, 0x7c517321)
#define VT_LOGICS_COLUMNOPERATION CKGUID(0x32351c65, 0x4f6916be)
#define VT_LOGICS_CREATEGROUPFROMARRAY CKGUID(0x22c71529, 0x6bb265d2)
#define VT_LOGICS_GETKEYROW CKGUID(0x49064205, 0x10e72f7a)
#define VT_LOGICS_SETCELL CKGUID(0x30ed1c6d, 0x4a3b7067)
#define VT_LOGICS_GETCELL CKGUID(0x33b99f51, 0x7d95c45)
#define VT_LOGICS_GETHIGHEST CKGUID(0x46f71e13, 0x13d26f61)
#define VT_LOGICS_GETLOWEST CKGUID(0x71b25ed7, 0x3c365a30)
#define VT_LOGICS_GETNEAREST CKGUID(0xfac60ad, 0x1bd31ac9)
#define VT_LOGICS_GETROW CKGUID(0x33b77f41, 0x7b95c45)
#define VT_LOGICS_GETCOLUMNNAME CKGUID(0x33b7ff41, 0x7b95c44)
#define VT_LOGICS_ITERATOR CKGUID(0x198f0af9, 0x268249f)
#define VT_LOGICS_ITERATORIF CKGUID(0x6bec4be6, 0x12d64c7c)
#define VT_LOGICS_ARRAYLOAD CKGUID(0x13bd2c64, 0x62db38e1)
#define VT_LOGICS_REMOVEKEYROW CKGUID(0xf8334ea, 0x279a40cd)
#define VT_LOGICS_REMOVEROW CKGUID(0x1fa57136, 0x14310857)
#define VT_LOGICS_REMOVEROWIF CKGUID(0x57865622, 0x662d2fee)
#define VT_LOGICS_REVERSEORDER CKGUID(0x3b7d3db7, 0x519f015b)
#define VT_LOGICS_ROWSEARCH CKGUID(0x78863443, 0x45af59b6)
#define VT_LOGICS_TESTCELL CKGUID(0x4e6c6da8, 0x636904fc)
#define VT_LOGICS_SETKEYROW CKGUID(0x2fff4c63, 0x4fcb1b94)
#define VT_LOGICS_SETROW CKGUID(0x62e87901, 0x2df007dd)
#define VT_LOGICS_SHUFFLEORDER CKGUID(0x270f177a, 0x4ca20023)
#define VT_LOGICS_SORTROWS CKGUID(0x6f623e68, 0x62bb5a98)
#define VT_LOGICS_SWAPROWS CKGUID(0x2ed72740, 0x406b5027)
#define VT_LOGICS_MAKEROWSUNIQUE CKGUID(0x2b175818, 0x2c270b20)
#define VT_LOGICS_VALUECOUNT CKGUID(0x534377de, 0x75fd478a)
#define VT_LOGICS_ARRAYSAVE CKGUID(0x15185846, 0x41014a45)
#define VT_LOGICS_REMOVECOLUMN CKGUID(0x3f377888, 0x128a7767)
#define VT_LOGICS_INSERTCOLUMN CKGUID(0x5cb7802, 0x4a64a58)
#define VT_LOGICS_MOVEROW CKGUID(0x101a62f4, 0x6e2d25f6)
#define VT_LOGICS_MOVECOLUMN CKGUID(0x1ace1319, 0x66b112b1)
#define VT_LOGICS_HASATTRIBUTE CKGUID(0x25b54079, 0x6ff90545)
#define VT_LOGICS_OBJECTSWITHATTRIBUTEITERATOR CKGUID(0x6bc1494c, 0xc816ad3)
#define VT_LOGICS_REMOVEATTRIBUTE CKGUID(0x6b6340c4, 0x61e94a41)
#define VT_LOGICS_SETATTRIBUTE CKGUID(0x373040f2, 0x5e01b34)
#define VT_LOGICS_HASATTRIBUTEOBSOLETE CKGUID(0x321e41c4, 0x6e41d5)
#define VT_LOGICS_REMOVEATTRIBUTEOBSOLETE CKGUID(0x321d56c4, 0x6d56d5)
#define VT_LOGICS_SETATTRIBUTEOBSOLETE CKGUID(0x321f32c4, 0x6f32d5)
#define VT_LOGICS_CREATESTRING CKGUID(0x4bcd2f9d, 0x382652e2)
#define VT_LOGICS_SCANSTRING CKGUID(0x4afa2f11, 0x34872e2)
#define VT_LOGICS_GETSUBSTRING CKGUID(0x10b16051, 0x26173b39)
#define VT_LOGICS_LOADSTRING CKGUID(0x391555d6, 0x42f2500e)
#define VT_LOGICS_REVERSESTRING CKGUID(0x72b17c2e, 0x644e2563)
#define VT_LOGICS_MODIFYCOMPONENT CKGUID(0xe5b234f3, 0x35fbacf7)
#define VT_LOGICS_CALCULATOR CKGUID(0x4bc209b8, 0x5b3679b0)
#define VT_LOGICS_GETCOMPONENT CKGUID(0x2689057b, 0x1a1a446a)
#define VT_LOGICS_GETDELTATIME CKGUID(0x47dc3232, 0x64bf203a)
#define VT_LOGICS_IDENTITY CKGUID(0x15151652, 0xaeefffd5)
#define VT_LOGICS_MINICALCULATOR CKGUID(0x55bc3115, 0x1dfe1e40)
#define VT_LOGICS_OP CKGUID(0x2d5d6d01, 0x6a353eb0)
#define VT_LOGICS_PERSECOND CKGUID(0x448e54ce, 0x75a655c5)
#define VT_LOGICS_RANDOM CKGUID(0xc622386, 0x1c3054f7)
#define VT_LOGICS_SETCOMPONENT CKGUID(0x6e800755, 0x57b64acb)
#define VT_LOGICS_THRESHOLD CKGUID(0x655e6af4, 0x8c0596f)
#define VT_LOGICS_VARIATION CKGUID(0x3e7335d9, 0x29687f36)
#define VT_LOGICS_BEZIERTRANSFORM CKGUID(0x6d433e1b, 0x191954b4)
#define VT_LOGICS_ADDOBJECTTOGROUP CKGUID(0x69aa320b, 0x23829c7)
#define VT_LOGICS_ADDTOGROUP CKGUID(0x24125, 0x785420ab)
#define VT_LOGICS_FILLGROUPBYCLASS CKGUID(0x4445257b, 0x70016c57)
#define VT_LOGICS_GROUPOPERATOR CKGUID(0x7b377625, 0x16da3bda)
#define VT_LOGICS_GROUPCLEAR CKGUID(0x1d12662e, 0x42687a3c)
#define VT_LOGICS_GETNEARESTINGROUP CKGUID(0x85207eb, 0x584950d8)
#define VT_LOGICS_GROUPITERATOR CKGUID(0x6050252f, 0x3aa82d40)
#define VT_LOGICS_ISINGROUP CKGUID(0x58a1210b, 0x14e5715c)
#define VT_LOGICS_REMOVEFROMGROUP CKGUID(0xd0147412, 0x258520d)
#define VT_LOGICS_REMOVEOBJECTFROMGROUP CKGUID(0x3c94747a, 0x50882825)
#define VT_LOGICS_BEZIERINTERPOLATOR CKGUID(0x71570e39, 0x5b37428a)
#define VT_LOGICS_INTERPOLATORHSVCOLOR CKGUID(0x778400bb, 0x1a480d35)
#define VT_LOGICS_INTERPOLATORCOLOR CKGUID(0x17171703, 0x17171703)
#define VT_LOGICS_INTERPOLATORFLOAT CKGUID(0x17171700, 0x17171700)
#define VT_LOGICS_INTERPOLATORINT CKGUID(0x17171701, 0x17171701)
#define VT_LOGICS_INTERPOLATORMATRIX CKGUID(0x3184d55, 0x7d7a0509)
#define VT_LOGICS_INTERPOLATOROBJECTORIENTATION CKGUID(0x17171704, 0x17171704)
#define VT_LOGICS_INTERPOLATORVECTOR CKGUID(0x17171702, 0x17171702)
#define VT_LOGICS_INTERPOLATOR CKGUID(0x35503950, 0x2dde7a65)
#define VT_LOGICS_TIMEBEZIERINTERPOLATOR CKGUID(0x34b2e0e, 0xcbf75e9)
#define VT_LOGICS_BEZIERPROGRESSION CKGUID(0x6bb8699a, 0x29fb6a4b)
#define VT_LOGICS_COUNTER CKGUID(0x998f000f, 0xf000f899)
#define VT_LOGICS_IBCQ CKGUID(0xf5f22514, 0x123fffee)
#define VT_LOGICS_LINEARPROGRESSION CKGUID(0xfff45680, 0xaa512a39)
#define VT_LOGICS_TIMER CKGUID(0xa2a5a63a, 0xe4e7e8e5)
#define VT_LOGICS_DELAYER CKGUID(0x15d472a5, 0x3bea409f)
#define VT_LOGICS_WHILE CKGUID(0x10015a6, 0xef597665)
#define VT_LOGICS_HIERARCHYPARSER CKGUID(0x5f4a214b, 0x6817452c)
#define VT_LOGICS_CHRONO CKGUID(0x5e2f1788, 0x4f965a44)
#define VT_LOGICS_COLLECTIONITERATOR CKGUID(0x419602ec, 0x7822779e)
#define VT_LOGICS_BROADCASTMESSAGE CKGUID(0x3d6c4ae1, 0x72ae2cd6)
#define VT_LOGICS_CHECKFORMESSAGE CKGUID(0x61226639, 0x6d390cb7)
#define VT_LOGICS_GETMESSAGEDATA CKGUID(0x45875fee, 0x45875fdd)
#define VT_LOGICS_SENDMESSAGE CKGUID(0xa20e8d5b, 0xdf002150)
#define VT_LOGICS_SENDMESSAGETOGROUP CKGUID(0x5f906952, 0x6df11649)
#define VT_LOGICS_SWITCHONMESSAGE CKGUID(0x1bb23f1d, 0x17ff14b9)
#define VT_LOGICS_WAITMESSAGE CKGUID(0x4587ffee, 0x4587ffdd)
#define VT_LOGICS_ALLBUTONE CKGUID(0x7ebc16ce, 0x75ba1301)
#define VT_LOGICS_BINARYMEMORY CKGUID(0xd02d67dd, 0x10211fdd)
#define VT_LOGICS_BINARYSWITCH CKGUID(0xeb506901, 0x984afccc)
#define VT_LOGICS_BOOLEVENT CKGUID(0xfefefefe, 0xfefefefe)
#define VT_LOGICS_FIFO CKGUID(0x7242794c, 0x98c2b340)
#define VT_LOGICS_LIFO CKGUID(0x7d037385, 0x12d62259)
#define VT_LOGICS_ONEATATIME CKGUID(0x271b61af, 0x41b44d43)
#define VT_LOGICS_PARAMETERSELECTOR CKGUID(0x63eb2b0c, 0x27e2767e)
#define VT_LOGICS_PRIORITY CKGUID(0xffb40d68, 0x45455444)
#define VT_LOGICS_RANDOMSWITCH CKGUID(0x79d72fde, 0x2e9d0912)
#define VT_LOGICS_SEQUENCER CKGUID(0x42530844, 0x257b6053)
#define VT_LOGICS_SPECIFICBOOLEVENT CKGUID(0xe5f22514, 0x12dfffee)
#define VT_LOGICS_STOPANDGO CKGUID(0x78a667f0, 0x5a3c0323)
#define VT_LOGICS_STREAMINGEVENT CKGUID(0x1f0b52bf, 0x4c3342dd)
#define VT_LOGICS_SWITCHONPARAMETER CKGUID(0x4c42aace, 0x1da45635)
#define VT_LOGICS_KEEPACTIVE CKGUID(0x7160133a, 0x1f2532fe)
#define VT_LOGICS_NOP CKGUID(0x302561c4, 0xd282980)
#define VT_LOGICS_TRIGGEREVENT CKGUID(0x3c3f7044, 0xe917d1a)
#define VT_LOGICS_ENTERCRITICALSECTION CKGUID(0x3c442002, 0x5988257c)
#define VT_LOGICS_LEAVECRITICALSECTION CKGUID(0x45db3138, 0x4db80f47)
#define VT_LOGICS_RENDEZVOUS CKGUID(0x3a1a16c7, 0x20095c45)
#define VT_LOGICS_WAITFORALL CKGUID(0xc044a999, 0xfdfefaf7)
#define VT_LOGICS_ISINVIEWFRUSTUM CKGUID(0x45542a7f, 0x4c2f01a6)
#define VT_LOGICS_OBJECTBETWEEN CKGUID(0x45484b53, 0x8eb1fab)
#define VT_LOGICS_PROXIMITY CKGUID(0x5321cacb, 0xdcdc5213)
#define VT_LOGICS_RAYINTERSECTION CKGUID(0x671e4a87, 0x383b2912)
#define VT_LOGICS_RAYBOXINTERSECTION CKGUID(0x671e5a87, 0x383b2372)
#define VT_LOGICS_TEST CKGUID(0x17d66d26, 0x726b7dec)

// Parameter Types
#define CKPGUID_INTERSECTIONPRECISIONTYPE CKGUID(0x6cf55733, 0x5af72dae)
#define CKPGUID_RECTBOXMODE CKGUID(0x5a6a3bd9, 0x7e2797d)
#define CKPGUID_PROXIMITY CKGUID(0x7fff5699, 0x7571336d)

#endif // BML_GUIDS_LOGICS_H
