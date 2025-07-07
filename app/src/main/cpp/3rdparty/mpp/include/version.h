/*
 * Copyright 2010 Rockchip Electronics S.LSI Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __VERSION_H__
#define __VERSION_H__

#define	MPP_VERSION             "1ea951af author: xiaoxu.chen   2025-07-01 fix[base]: Fix enc cfg size"
#define	MPP_VER_HIST_CNT        10
#define	MPP_VER_HIST_0          "1ea951af author: xiaoxu.chen   2025-07-01 fix[base]: Fix enc cfg size  (HEAD -> develop, origin/develop, origin/HEAD)"
#define	MPP_VER_HIST_1          "6d8d29bb author: Hongjin Li    2025-06-30 fix[mpp_sys_cfg]: fix RK3399 hor stride calc issue"
#define	MPP_VER_HIST_2          "04a695b8 author: Johnson Ding  2025-06-27 chore[dec_test]: Add FBC output support"
#define	MPP_VER_HIST_3          "e08da1bd author: Yandong Lin   2025-06-25 fix[av1d_parser]: Fix parse obu units error."
#define	MPP_VER_HIST_4          "07580ac7 author: timkingh.huang 2025-06-25 fix[hal_h265e]: Fix crash on zero gop"
#define	MPP_VER_HIST_5          "92ca648b author: timkingh.huang 2025-06-24 feat[smt_v3]: Add parameters cfg interface"
#define	MPP_VER_HIST_6          "560ac10b author: timkingh.huang 2025-06-24 feat[vepu510]: Add smart v3 interface"
#define	MPP_VER_HIST_7          "fb6f1703 author: Herman Chen   2025-06-24 fix[base]: Fix packet and frame pool init issue"
#define	MPP_VER_HIST_8          "a0567034 author: yichen.wang   2025-06-24 feat[mpp_enc_cfg]: Add H.264/H.265 vui enable cfg"
#define	MPP_VER_HIST_9          "ccee46d0 author: Herman Chen   2025-06-24 fix[cmake]: Fix compile error on linux with asan"

#endif /*__VERSION_H__*/
