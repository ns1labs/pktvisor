/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

// author Kevin Lang, Oath Research

#ifndef ICON_ESTIMATOR_HPP_
#define ICON_ESTIMATOR_HPP_

#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace datasketches {

// The ICON estimator for FM85 sketches is defined by the arXiv paper.

// The current file provides exact and approximate implementations of this estimator.

// The exact version works for any value of K, but is quite slow.

// The much faster approximate version works for K values that are powers of two
// ranging from 2^4 to 2^32.

// At a high-level, this approximation can be described as using an
// exponential approximation when C > K * (5.6 or 5.7), while smaller
// values of C are handled by a degree-19 polynomial approximation of
// a pre-conditioned version of the true ICON mapping from C to N_hat.

// This file also provides a validation procedure that compares its approximate
// and exact implementations of the FM85 ICON estimator.

static const int ICON_MIN_LOG_K = 4;
static const int ICON_MAX_LOG_K = 26;
static const int ICON_POLYNOMIAL_DEGREE = 19;
static const int ICON_POLYNOMIAL_NUM_COEFFICIENTS = 1 + ICON_POLYNOMIAL_DEGREE;
static const int ICON_TABLE_SIZE = ICON_POLYNOMIAL_NUM_COEFFICIENTS * (1 + (ICON_MAX_LOG_K - ICON_MIN_LOG_K));

static const double ICON_POLYNOMIAL_COEFFICIENTS[ICON_TABLE_SIZE] = {

 // log K = 4
 0.9895027971889700513, 0.3319496644645180128, 0.1242818722715769986, -0.03324149686026930256, -0.2985637298081619817,
 1.366555923595830002, -4.705499366260569971, 11.61506432505530029, -21.11254986175579873, 28.89421695078809904,
 -30.1383659011730991, 24.11946778830730054, -14.83391445199539938, 6.983088767267210173, -2.48964120264876998,
 0.6593243603602499947, -0.125493534558034997, 0.01620971672896159843, -0.001271267679036929953, 4.567178653294529745e-05,

 // log K = 5
 0.9947713741300230339, 0.3326559581620939787, 0.1250050661634889981, -0.04130073804472530336, -0.2584095537451129854,
 1.218050389433120051, -4.319106696095399656, 10.87175052045090062, -20.0184979022142997, 27.63210188163320069,
 -28.97950009664030091, 23.26740804691930009, -14.33375703270860058, 6.751281271241110105, -2.406363094133439962,
 0.6367414734718820357, -0.1210468076141379967, 0.01561196698118279963, -0.001222335432128580056, 4.383502970318410206e-05,

 // log K = 6
 0.9973904854982870161, 0.3330148852217920119, 0.125251536589509993, -0.04434075124043219962, -0.2436238890691720116,
 1.163293254754570016, -4.177758779777369647, 10.60301981340099964, -19.6274507428828997, 27.18420839597660077,
 -28.56827214174580121, 22.96268674086600114, -14.15234202220280046, 6.665700662642549901, -2.375043356720739851,
 0.6280993991240929608, -0.119319019358031006, 0.01537674055733759954, -0.001202881695730769916, 4.309894633186929849e-05,

 // log K = 7
 0.9986963310058679655, 0.3331956705633329907, 0.125337696770523005, -0.04546817338088020299, -0.2386752211125199863,
 1.145927328111949972, -4.135694445582720036, 10.52805060502839929, -19.52408322548339825, 27.06921653903929936,
 -28.46207532143190022, 22.88083524357429965, -14.10057147392659971, 6.63958754983273991, -2.364865219283200037,
 0.6251341806425250169, -0.1186991327450530043, 0.0152892726403408008, -0.001195439764873199896, 4.281098416794090072e-05,

 // log K = 8
 0.999348600452531044, 0.3332480372393080148, 0.126666900963325002, -0.06495714694254159371, -0.08376282050638980681,
 0.3760158094643630267, -1.568204791601850001, 4.483117719555970382, -9.119180124379150598, 13.65799293358900002,
 -15.3100211234349004, 12.97546344654869976, -8.351661538536939489, 4.075022612435580172, -1.49387015887069996,
 0.4040976870253379927, -0.07813232681879349328, 0.01020545649538820085, -0.0008063279210812720381, 2.909334976414100078e-05,

 // log K = 9
 0.9996743787297059924, 0.3332925779481850093, 0.1267124599259649986, -0.06550452970936600228, -0.08191738117533520214,
 0.3773034458363569987, -1.604679509609959975, 4.636761898691969641, -9.487348609558699408, 14.25164235443030059,
 -15.99674955529870068, 13.56353219046370029, -8.730194904342459594, 4.259010067932120336, -1.56106689792022002,
 0.4222540912786589828, -0.08165296504921559784, 0.01066878484925220041, -0.0008433887618256910015, 3.045339724886519912e-05,

 // log K = 10
 0.999837191783945034, 0.3333142252339619804, 0.1267759538087240012, -0.06631005632753710077, -0.07692759158286699428,
 0.3568943956395980166, -1.546598721379510044, 4.51595019978557044, -9.298431968763770428, 14.02586858080080034,
 -15.78858959520439953, 13.41484931677589998, -8.647958125130809748, 4.22398017468472009, -1.549708891200570093,
 0.419507410264540026, -0.08117411611046250475, 0.01061202286184199928, -0.000839300527596772007, 3.03185874520205985e-05,

 // log K = 11
 0.9999186020796150265, 0.3333249054574359826, 0.126791713589799987, -0.06662487271699729652, -0.07335552427910230211,
 0.3316370184815959909, -1.434143797561290068, 4.180260309967409604, -8.593906870708760692, 12.95088874800289958,
 -14.56876092520539956, 12.37074367531410068, -7.969152075707960137, 3.888774396648960074, -1.424923326506990051,
 0.385084561785229984, -0.07435541911616409816, 0.009695363567476529554, -0.0007644375960047160388, 2.75156194717188011e-05,

 // log K = 12
 0.9999592955649559967, 0.3333310560725140093, 0.1267379744020450116, -0.06524495415766619344, -0.08854031542298740343,
 0.4244320628874230228, -1.794077789033230008, 5.133875262768450298, -10.40149374917120007, 15.47808115629240078,
 -17.2272296137545986, 14.5002173676463002, -9.274819801602760094, 4.500782540026570189, -1.642359389030050076,
 0.442596113445525019, -0.0853226219238850947, 0.01111969379054169975, -0.0008771614088006969611, 3.161668519459719752e-05,

 // log K = 13
 0.9999796468102559732, 0.3333336602394039727, 0.126728089053198989, -0.06503798598282370391, -0.09050261023823169548,
 0.4350609244189960201, -1.831274835815670077, 5.223387516985289913, -10.55574395269979959, 15.67359470222429962,
 -17.41263416341029924, 14.63297400889229927, -9.346752431221359458, 4.530124905188380069, -1.651245566462089975,
 0.444542549250713015, -0.08561720963336499901, 0.01114805146185449992, -0.0008786251203363140043, 3.16416341644572998e-05,

 // log K = 14
 0.9999898187060970445, 0.3333362579300819806, 0.1266984078369459976, -0.06464561179765909715, -0.09343280886228019777,
 0.4490702549264070087, -1.878087608052450008, 5.338004322057390283, -10.76690603590630069, 15.97069195083200022,
 -17.73440379943459888, 14.90212518309260048, -9.520506013770420495, 4.616238931978830173, -1.68364817877918993,
 0.4536194960681350086, -0.087448605434800597, 0.01139929991331390009, -0.0008995891451622229631, 3.244407259782900338e-05,

 // log K = 15
 0.9999949072549390028, 0.3333376334705290267, 0.126665364358402005, -0.06411790034705669439, -0.09776009134670660128,
 0.4704691112248470253, -1.948021675295769972, 5.497760972696490001, -11.03165645315390009, 16.29703330781000048,
 -18.03851029448010124, 15.11836776139680083, -9.638205179917429533, 4.665122328753120051, -1.698980686525759953,
 0.4571799506245269873, -0.08804011353783609828, 0.01146553155965330043, -0.0009040455800659569869, 3.257931866957050274e-05,

 // log K = 16
 0.9999974544793589493, 0.3333381337614599871, 0.1266524862971120102, -0.06391676499117690535, -0.09929616211306059592,
 0.4771390820378790254, -1.965762451227349938, 5.526802350376460282, -11.05703067024660058, 16.29535848023060041,
 -18.00114005075790047, 15.06214012231560062, -9.58874727382628933, 4.63537541652793017, -1.686222848555620102,
 0.4532602373715179933, -0.08719448925964939923, 0.01134365425717459921, -0.0008934965241274289835, 3.216436244471380105e-05,

 // log K = 17
 0.9999987278278800185, 0.3333383411464330148, 0.126642761751724009, -0.06371042959073920653, -0.1013564516034080043,
 0.4891311195679299839, -2.010971712051409899, 5.644390807952309963, -11.27697253921500042, 16.59957157207080058,
 -18.31808338317799922, 15.31363518393730061, -9.741451446816620674, 4.706207545519429658, -1.711102469010010063,
 0.4597587341089349744, -0.08841670767182820134, 0.01149999225097850068, -0.0009056651366963050422, 3.259910736274500059e-05,

 // log K = 18
 0.9999993637727100371, 0.3333385511608860097, 0.1266341580529160016, -0.06353272828164230335, -0.103139962850642003,
 0.4996216017206500104, -2.05099128585287982, 5.749874086531799655, -11.47727638570349917, 16.88141587810320132,
 -18.61744656177490143, 15.55634230427719977, -9.892350736128680211, 4.778033520984200422, -1.737045483861280104,
 0.4667410882683730167, -0.08977256212421590165, 0.01167940146667079994, -0.0009201381242396030127, 3.313600701586759867e-05,

 // log K = 19
 0.9999996805376010212, 0.3333372324328989778, 0.1267104737214659882, -0.06504749929326139601, -0.0882341962464350954,
 0.4131871162041140244, -1.725190703567099915, 4.900817515593920426, -9.883452720776510603, 14.6657081190816001,
 -16.29398295135089825, 13.69805011761319946, -8.753475239465899449, 4.244072374564439976, -1.547202527706629915,
 0.4164770109614310267, -0.08017596922092029565, 0.01043146101701039954, -0.00082124200571200305, 2.953319493719429935e-05,

 // log K = 20
 0.9999998390037539986, 0.3333365859956040067, 0.1267460211029839967, -0.06569456024647769843, -0.0823070353477164951,
 0.3810826463303410017, -1.611983580241109992, 4.624520077758210057, -9.397308335633589138, 14.03184981378050011,
 -15.6703191315401007, 13.22992718704790072, -8.484216393184780713, 4.125607133488029987, -1.507690650697159906,
 0.4066678517577320129, -0.07842110121777939868, 0.01021780862225150042, -0.0008054065857047439754, 2.899431830426989844e-05,

 // log K = 21
 0.9999999207001479817, 0.3333384953015239849, 0.1266331480396669928, -0.06345750166298599892, -0.1042341210992499961,
 0.5077112908497130039, -2.087398133609810191, 5.858842546192500222, -11.70620319777190055, 17.23103975433669888,
 -19.01462552846669851, 15.89674059836560005, -10.11395134034419918, 4.88760796465891989, -1.777886770904629987,
 0.4780200178339499839, -0.09200895321782050218, 0.01198029553244219989, -0.0009447283875782100165, 3.405716775824710232e-05,

 // log K = 22
 0.9999999606908690497, 0.3333383929524300071, 0.1266456445096819927, -0.06373504294081690225, -0.1012834291081849969,
 0.4893810690172959998, -2.01391428223606983, 5.656430437473649597, -11.3067201537791, 16.64980594135310099,
 -18.3792355790383013, 15.36879753115040081, -9.778831246425049528, 4.725308061988969577, -1.718423596500280093,
 0.4618308177809870019, -0.08883675060799739454, 0.01155766944804260087, -0.0009104695617243750358, 3.278237729674439666e-05,

 // log K = 23
 0.9999999794683379628, 0.3333386441751680085, 0.1266463995182049995, -0.06376031920455070556, -0.1010799540803130059,
 0.488540137426137, -2.012048323537570127, 5.654949475342659682, -11.31023240892979942, 16.66334675284959843,
 -18.40241452866079896, 15.39443572867130072, -9.798844412838670692, 4.736683907539640082, -1.723168363744929987,
 0.463270349018644001, -0.08914619066708899531, 0.01160235936257320022, -0.0009143600818183229709, 3.293669304679140117e-05,

 // log K = 24
 0.9999999911469820146, 0.3333376076934529975, 0.1266944349940530012, -0.06470524278387919381, -0.09189342220283110152,
 0.4359182372694809793, -1.815980282951169977, 5.149474056470340066, -10.37086570678100017, 15.36962686758569951,
 -17.05756384717849983, 14.32755177515199918, -9.149944050025640152, 4.434601894497260055, -1.616478926806520056,
 0.4351979157055039793, -0.08381768225272340223, 0.01091321820476520016, -0.0008600264403629039739, 3.09667800347144002e-05,

 // log K = 25
 0.9999999968592140354, 0.3333379164881000167, 0.1266782495827009913, -0.06434163088961859789, -0.09575258124988890451,
 0.4597843575354370049, -1.911374431241559924, 5.411856661251520428, -10.88850084646090011, 16.12298941380269923,
 -17.88172178487259956, 15.01301780636859995, -9.585542896142529301, 4.645811872761620442, -1.693952293156189892,
 0.4563143308861309921, -0.08795976148455289523, 0.01146560428011200033, -0.0009048442931930629528, 3.26358391497329992e-05,

 // log K = 26
 0.9999999970700530483, 0.333338329556315982, 0.126644753076394001, -0.06372365346512399997, -0.1012760856945769949,
 0.4886852278576360176, -2.009005418394389952, 5.638119224137019714, -11.26276715335160006, 16.57640024218650154,
 -18.29035093605569884, 15.28892246224570073, -9.724916375991760731, 4.6978877652334603, -1.707974125916829955,
 0.4588937864564729963, -0.08824617586088029375, 0.01147732114826570046, -0.00090384524860747295, 3.253252703695579795e-05,

#ifdef LARGER_K_VALUES
 // log K = 27
 1.000000000639100106, 0.3333378987508219815, 0.126670943746902992, -0.06418811974745139426, -0.0972951198506895043,
 0.4687977077401049852, -1.945290489888900076, 5.499494964974400268, -11.05078190574979935, 16.3446428009706004,
 -18.10936908931320133, 15.19089294103859977, -9.691829972777059155, 4.694320543263319934, -1.710719212277360013,
 0.4606257962161550146, -0.08875858006645380438, 0.01156634964444109952, -0.0009125838337464230437, 3.290907977404550287e-05,

 // log K = 28
 0.9999999993590269476, 0.3333385660745579737, 0.1266394134278630013, -0.0636305053404186971, -0.1022354305220320031,
 0.4945787360853979853, -2.032468917547570086, 5.702461924065530319, -11.38943406618639997, 16.76052144140630062,
 -18.49169753114890113, 15.4564578116809006, -9.831507534599410292, 4.749667961030789698, -1.72701519749717991,
 0.4640997252013580043, -0.08927103511252110213, 0.01161455495023329919, -0.000915030036039231982, 3.295110296010450275e-05,

 // log K = 29
 0.9999999998441060356, 0.3333383341194189886, 0.1266687338487519909, -0.06416245828383730643, -0.09764561286937140094,
 0.4715274747139350242, -1.958172229464169911, 5.539587632966780362, -11.13784217611559946, 16.48149277721759987,
 -18.26888916646990069, 15.33085193018819936, -9.78493991484172021, 4.741302923579859829, -1.728568959451310061,
 0.4656457646521020011, -0.08977142058582450457, 0.01170492245846839995, -0.0009240931538567209464, 3.334703207098030245e-05,

 // log K = 30
 0.9999999992599339915, 0.3333384538468979752, 0.1266452025739940035, -0.06374775920488300052, -0.1009917742909720029,
 0.4867931642504759737, -2.000981224888669807, 5.614968747087539569, -11.21527907219130071, 16.50500949673639894,
 -18.21007853829650003, 15.22056128176249956, -9.680565515478869898, 4.675983737170599674, -1.69980511941418011,
 0.4566332138743600111, -0.08779650251621799739, 0.01141656381272189956, -0.0008988545845624889468, 3.234448025291899689e-05,

 // log K = 31
 0.9999999973204000137, 0.333337762450663988, 0.1266965469104399944, -0.06475154253624139378, -0.09133098208494490333,
 0.4320356889637699815, -1.799236887220760028, 5.100971076171499696, -10.27175516606700079, 15.22198757843720074,
 -16.89368636262300072, 14.19016571851859965, -9.062390133299189188, 4.39220025249522994, -1.600994848692480099,
 0.4310075283759189912, -0.08300339267288289746, 0.01080584419810979961, -0.0008514267355136160122, 3.065110087496039805e-05,

 // log K = 32
 0.9999999987706390536, 0.3333387038350890119, 0.1266354589419070031, -0.06355195838981600454, -0.102952771506954005,
 0.4983589546197609854, -2.045281215270029929, 5.732181222451769642, -11.43849817800069957, 16.81961198331340057,
 -18.54433120118400069, 15.49126422718470053, -9.84846998787154071, 4.755615082534379923, -1.728430514092559989,
 0.4642927653670489985, -0.08927380119154580684, 0.01161055316485629964, -0.0009143724787632470305, 3.291492066818770055e-05,

#endif
};

static inline double evaluate_polynomial(const double* coefficients, int start, int num, double x) {
  const int final = start + num - 1;
  double total = coefficients[final];
  for (int j = final - 1; j >= start; j--) {
    total *= x;
    total += coefficients[j];
  }
  return total;
}

static inline double icon_exponential_approximation(double k, double c) {
  return (0.7940236163830469 * k * pow(2.0, c / k));
}

static inline double compute_icon_estimate(uint8_t lg_k, uint32_t c) {
  if (lg_k < ICON_MIN_LOG_K || lg_k > ICON_MAX_LOG_K) throw std::out_of_range("lg_k out of range");
  if (c < 2) return ((c == 0) ? 0.0 : 1.0);
  const uint32_t k = 1 << lg_k;
  const double double_k = static_cast<double>(k);
  const double double_c = static_cast<double>(c);
  // Differing thresholds ensure that the approximated estimator is monotonically increasing.
  const double threshold_factor = ((lg_k < 14) ? 5.7 : 5.6);
  if (double_c > (threshold_factor * double_k)) return icon_exponential_approximation(double_k, double_c);
  const double factor = evaluate_polynomial(
      ICON_POLYNOMIAL_COEFFICIENTS,
      ICON_POLYNOMIAL_NUM_COEFFICIENTS * (lg_k - ICON_MIN_LOG_K),
      ICON_POLYNOMIAL_NUM_COEFFICIENTS,
      // The somewhat arbitrary constant 2.0 is baked into the table ICON_POLYNOMIAL_COEFFICIENTS
      double_c / (2.0 * double_k)
  );
  const double ratio = double_c / double_k;
  // The somewhat arbitrary constant 66.774757 is baked into the table ICON_POLYNOMIAL_COEFFICIENTS
  const double term = 1.0 + (ratio * ratio * ratio / 66.774757);
  const double result = double_c * factor * term;
  if (result >= double_c) return result;
  else return double_c;
}

} /* namespace datasketches */

#endif
