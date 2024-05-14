int sindeg[] =
{
	0, // 0 0.000000
	-285, // 1 -0.017452
	-571, // 2 -0.034899
	-857, // 3 -0.052336
	-1142, // 4 -0.069756
	-1427, // 5 -0.087156
	-1712, // 6 -0.104528
	-1996, // 7 -0.121869
	-2280, // 8 -0.139173
	-2563, // 9 -0.156434
	-2845, // 10 -0.173648
	-3126, // 11 -0.190809
	-3406, // 12 -0.207912
	-3685, // 13 -0.224951
	-3963, // 14 -0.241922
	-4240, // 15 -0.258819
	-4516, // 16 -0.275637
	-4790, // 17 -0.292372
	-5062, // 18 -0.309017
	-5334, // 19 -0.325568
	-5603, // 20 -0.342020
	-5871, // 21 -0.358368
	-6137, // 22 -0.374607
	-6401, // 23 -0.390731
	-6663, // 24 -0.406737
	-6924, // 25 -0.422618
	-7182, // 26 -0.438371
	-7438, // 27 -0.453990
	-7691, // 28 -0.469472
	-7943, // 29 -0.484810
	-8191, // 30 -0.500000
	-8438, // 31 -0.515038
	-8682, // 32 -0.529919
	-8923, // 33 -0.544639
	-9161, // 34 -0.559193
	-9397, // 35 -0.573576
	-9630, // 36 -0.587785
	-9860, // 37 -0.601815
	-10086, // 38 -0.615661
	-10310, // 39 -0.629320
	-10531, // 40 -0.642788
	-10748, // 41 -0.656059
	-10963, // 42 -0.669131
	-11173, // 43 -0.681998
	-11381, // 44 -0.694658
	-11585, // 45 -0.707107
	-11785, // 46 -0.719340
	-11982, // 47 -0.731354
	-12175, // 48 -0.743145
	-12365, // 49 -0.754710
	-12550, // 50 -0.766044
	-12732, // 51 -0.777146
	-12910, // 52 -0.788011
	-13084, // 53 -0.798636
	-13254, // 54 -0.809017
	-13420, // 55 -0.819152
	-13582, // 56 -0.829038
	-13740, // 57 -0.838671
	-13894, // 58 -0.848048
	-14043, // 59 -0.857167
	-14188, // 60 -0.866025
	-14329, // 61 -0.874620
	-14466, // 62 -0.882948
	-14598, // 63 -0.891007
	-14725, // 64 -0.898794
	-14848, // 65 -0.906308
	-14967, // 66 -0.913545
	-15081, // 67 -0.920505
	-15190, // 68 -0.927184
	-15295, // 69 -0.933580
	-15395, // 70 -0.939693
	-15491, // 71 -0.945519
	-15582, // 72 -0.951057
	-15668, // 73 -0.956305
	-15749, // 74 -0.961262
	-15825, // 75 -0.965926
	-15897, // 76 -0.970296
	-15964, // 77 -0.974370
	-16025, // 78 -0.978148
	-16082, // 79 -0.981627
	-16135, // 80 -0.984808
	-16182, // 81 -0.987688
	-16224, // 82 -0.990268
	-16261, // 83 -0.992546
	-16294, // 84 -0.994522
	-16321, // 85 -0.996195
	-16344, // 86 -0.997564
	-16361, // 87 -0.998630
	-16374, // 88 -0.999391
	-16381, // 89 -0.999848
	-16384, // 90 -1.000000
	-16381, // 91 -0.999848
	-16374, // 92 -0.999391
	-16361, // 93 -0.998630
	-16344, // 94 -0.997564
	-16321, // 95 -0.996195
	-16294, // 96 -0.994522
	-16261, // 97 -0.992546
	-16224, // 98 -0.990268
	-16182, // 99 -0.987688
	-16135, // 100 -0.984808
	-16082, // 101 -0.981627
	-16025, // 102 -0.978148
	-15964, // 103 -0.974370
	-15897, // 104 -0.970296
	-15825, // 105 -0.965926
	-15749, // 106 -0.961262
	-15668, // 107 -0.956305
	-15582, // 108 -0.951057
	-15491, // 109 -0.945519
	-15395, // 110 -0.939693
	-15295, // 111 -0.933580
	-15190, // 112 -0.927184
	-15081, // 113 -0.920505
	-14967, // 114 -0.913545
	-14848, // 115 -0.906308
	-14725, // 116 -0.898794
	-14598, // 117 -0.891007
	-14466, // 118 -0.882948
	-14329, // 119 -0.874620
	-14188, // 120 -0.866025
	-14043, // 121 -0.857167
	-13894, // 122 -0.848048
	-13740, // 123 -0.838671
	-13582, // 124 -0.829038
	-13420, // 125 -0.819152
	-13254, // 126 -0.809017
	-13084, // 127 -0.798636
	-12910, // 128 -0.788011
	-12732, // 129 -0.777146
	-12550, // 130 -0.766044
	-12365, // 131 -0.754710
	-12175, // 132 -0.743145
	-11982, // 133 -0.731354
	-11785, // 134 -0.719340
	-11585, // 135 -0.707107
	-11381, // 136 -0.694658
	-11173, // 137 -0.681998
	-10963, // 138 -0.669131
	-10748, // 139 -0.656059
	-10531, // 140 -0.642788
	-10310, // 141 -0.629320
	-10086, // 142 -0.615661
	-9860, // 143 -0.601815
	-9630, // 144 -0.587785
	-9397, // 145 -0.573576
	-9161, // 146 -0.559193
	-8923, // 147 -0.544639
	-8682, // 148 -0.529919
	-8438, // 149 -0.515038
	-8191, // 150 -0.500000
	-7943, // 151 -0.484810
	-7691, // 152 -0.469472
	-7438, // 153 -0.453990
	-7182, // 154 -0.438371
	-6924, // 155 -0.422618
	-6663, // 156 -0.406737
	-6401, // 157 -0.390731
	-6137, // 158 -0.374607
	-5871, // 159 -0.358368
	-5603, // 160 -0.342020
	-5334, // 161 -0.325568
	-5062, // 162 -0.309017
	-4790, // 163 -0.292372
	-4516, // 164 -0.275637
	-4240, // 165 -0.258819
	-3963, // 166 -0.241922
	-3685, // 167 -0.224951
	-3406, // 168 -0.207912
	-3126, // 169 -0.190809
	-2845, // 170 -0.173648
	-2563, // 171 -0.156434
	-2280, // 172 -0.139173
	-1996, // 173 -0.121869
	-1712, // 174 -0.104528
	-1427, // 175 -0.087156
	-1142, // 176 -0.069756
	-857, // 177 -0.052336
	-571, // 178 -0.034899
	-285, // 179 -0.017452
	0, // 180 -0.000000
	285, // 181 0.017452
	571, // 182 0.034899
	857, // 183 0.052336
	1142, // 184 0.069756
	1427, // 185 0.087156
	1712, // 186 0.104528
	1996, // 187 0.121869
	2280, // 188 0.139173
	2563, // 189 0.156434
	2845, // 190 0.173648
	3126, // 191 0.190809
	3406, // 192 0.207912
	3685, // 193 0.224951
	3963, // 194 0.241922
	4240, // 195 0.258819
	4516, // 196 0.275637
	4790, // 197 0.292372
	5062, // 198 0.309017
	5334, // 199 0.325568
	5603, // 200 0.342020
	5871, // 201 0.358368
	6137, // 202 0.374607
	6401, // 203 0.390731
	6663, // 204 0.406737
	6924, // 205 0.422618
	7182, // 206 0.438371
	7438, // 207 0.453990
	7691, // 208 0.469472
	7943, // 209 0.484810
	8192, // 210 0.500000
	8438, // 211 0.515038
	8682, // 212 0.529919
	8923, // 213 0.544639
	9161, // 214 0.559193
	9397, // 215 0.573576
	9630, // 216 0.587785
	9860, // 217 0.601815
	10086, // 218 0.615661
	10310, // 219 0.629320
	10531, // 220 0.642788
	10748, // 221 0.656059
	10963, // 222 0.669131
	11173, // 223 0.681998
	11381, // 224 0.694658
	11585, // 225 0.707107
	11785, // 226 0.719340
	11982, // 227 0.731354
	12175, // 228 0.743145
	12365, // 229 0.754710
	12550, // 230 0.766044
	12732, // 231 0.777146
	12910, // 232 0.788011
	13084, // 233 0.798636
	13254, // 234 0.809017
	13420, // 235 0.819152
	13582, // 236 0.829038
	13740, // 237 0.838671
	13894, // 238 0.848048
	14043, // 239 0.857167
	14188, // 240 0.866025
	14329, // 241 0.874620
	14466, // 242 0.882948
	14598, // 243 0.891007
	14725, // 244 0.898794
	14848, // 245 0.906308
	14967, // 246 0.913545
	15081, // 247 0.920505
	15190, // 248 0.927184
	15295, // 249 0.933580
	15395, // 250 0.939693
	15491, // 251 0.945519
	15582, // 252 0.951057
	15668, // 253 0.956305
	15749, // 254 0.961262
	15825, // 255 0.965926
	15897, // 256 0.970296
	15964, // 257 0.974370
	16025, // 258 0.978148
	16082, // 259 0.981627
	16135, // 260 0.984808
	16182, // 261 0.987688
	16224, // 262 0.990268
	16261, // 263 0.992546
	16294, // 264 0.994522
	16321, // 265 0.996195
	16344, // 266 0.997564
	16361, // 267 0.998630
	16374, // 268 0.999391
	16381, // 269 0.999848
	16384, // 270 1.000000
	16381, // 271 0.999848
	16374, // 272 0.999391
	16361, // 273 0.998630
	16344, // 274 0.997564
	16321, // 275 0.996195
	16294, // 276 0.994522
	16261, // 277 0.992546
	16224, // 278 0.990268
	16182, // 279 0.987688
	16135, // 280 0.984808
	16082, // 281 0.981627
	16025, // 282 0.978148
	15964, // 283 0.974370
	15897, // 284 0.970296
	15825, // 285 0.965926
	15749, // 286 0.961262
	15668, // 287 0.956305
	15582, // 288 0.951057
	15491, // 289 0.945519
	15395, // 290 0.939693
	15295, // 291 0.933580
	15190, // 292 0.927184
	15081, // 293 0.920505
	14967, // 294 0.913545
	14848, // 295 0.906308
	14725, // 296 0.898794
	14598, // 297 0.891007
	14466, // 298 0.882948
	14329, // 299 0.874620
	14188, // 300 0.866025
	14043, // 301 0.857167
	13894, // 302 0.848048
	13740, // 303 0.838671
	13582, // 304 0.829038
	13420, // 305 0.819152
	13254, // 306 0.809017
	13084, // 307 0.798636
	12910, // 308 0.788011
	12732, // 309 0.777146
	12550, // 310 0.766044
	12365, // 311 0.754710
	12175, // 312 0.743145
	11982, // 313 0.731354
	11785, // 314 0.719340
	11585, // 315 0.707107
	11381, // 316 0.694658
	11173, // 317 0.681998
	10963, // 318 0.669131
	10748, // 319 0.656059
	10531, // 320 0.642788
	10310, // 321 0.629320
	10086, // 322 0.615661
	9860, // 323 0.601815
	9630, // 324 0.587785
	9397, // 325 0.573576
	9161, // 326 0.559193
	8923, // 327 0.544639
	8682, // 328 0.529919
	8438, // 329 0.515038
	8192, // 330 0.500000
	7943, // 331 0.484810
	7691, // 332 0.469472
	7438, // 333 0.453990
	7182, // 334 0.438371
	6924, // 335 0.422618
	6663, // 336 0.406737
	6401, // 337 0.390731
	6137, // 338 0.374607
	5871, // 339 0.358368
	5603, // 340 0.342020
	5334, // 341 0.325568
	5062, // 342 0.309017
	4790, // 343 0.292372
	4516, // 344 0.275637
	4240, // 345 0.258819
	3963, // 346 0.241922
	3685, // 347 0.224951
	3406, // 348 0.207912
	3126, // 349 0.190809
	2845, // 350 0.173648
	2563, // 351 0.156434
	2280, // 352 0.139173
	1996, // 353 0.121869
	1712, // 354 0.104528
	1427, // 355 0.087156
	1142, // 356 0.069756
	857, // 357 0.052336
	571, // 358 0.034899
	285, // 359 0.017452
	0 // 360 0.000000
};

int cosdeg[] =
{
	16384, // 0 1.000000
	16381, // 1 0.999848
	16374, // 2 0.999391
	16361, // 3 0.998630
	16344, // 4 0.997564
	16321, // 5 0.996195
	16294, // 6 0.994522
	16261, // 7 0.992546
	16224, // 8 0.990268
	16182, // 9 0.987688
	16135, // 10 0.984808
	16082, // 11 0.981627
	16025, // 12 0.978148
	15964, // 13 0.974370
	15897, // 14 0.970296
	15825, // 15 0.965926
	15749, // 16 0.961262
	15668, // 17 0.956305
	15582, // 18 0.951057
	15491, // 19 0.945519
	15395, // 20 0.939693
	15295, // 21 0.933580
	15190, // 22 0.927184
	15081, // 23 0.920505
	14967, // 24 0.913545
	14848, // 25 0.906308
	14725, // 26 0.898794
	14598, // 27 0.891007
	14466, // 28 0.882948
	14329, // 29 0.874620
	14188, // 30 0.866025
	14043, // 31 0.857167
	13894, // 32 0.848048
	13740, // 33 0.838671
	13582, // 34 0.829038
	13420, // 35 0.819152
	13254, // 36 0.809017
	13084, // 37 0.798636
	12910, // 38 0.788011
	12732, // 39 0.777146
	12550, // 40 0.766044
	12365, // 41 0.754710
	12175, // 42 0.743145
	11982, // 43 0.731354
	11785, // 44 0.719340
	11585, // 45 0.707107
	11381, // 46 0.694658
	11173, // 47 0.681998
	10963, // 48 0.669131
	10748, // 49 0.656059
	10531, // 50 0.642788
	10310, // 51 0.629320
	10086, // 52 0.615661
	9860, // 53 0.601815
	9630, // 54 0.587785
	9397, // 55 0.573576
	9161, // 56 0.559193
	8923, // 57 0.544639
	8682, // 58 0.529919
	8438, // 59 0.515038
	8192, // 60 0.500000
	7943, // 61 0.484810
	7691, // 62 0.469472
	7438, // 63 0.453990
	7182, // 64 0.438371
	6924, // 65 0.422618
	6663, // 66 0.406737
	6401, // 67 0.390731
	6137, // 68 0.374607
	5871, // 69 0.358368
	5603, // 70 0.342020
	5334, // 71 0.325568
	5062, // 72 0.309017
	4790, // 73 0.292372
	4516, // 74 0.275637
	4240, // 75 0.258819
	3963, // 76 0.241922
	3685, // 77 0.224951
	3406, // 78 0.207912
	3126, // 79 0.190809
	2845, // 80 0.173648
	2563, // 81 0.156434
	2280, // 82 0.139173
	1996, // 83 0.121869
	1712, // 84 0.104528
	1427, // 85 0.087156
	1142, // 86 0.069756
	857, // 87 0.052336
	571, // 88 0.034899
	285, // 89 0.017452
	0, // 90 0.000000
	-285, // 91 -0.017452
	-571, // 92 -0.034899
	-857, // 93 -0.052336
	-1142, // 94 -0.069756
	-1427, // 95 -0.087156
	-1712, // 96 -0.104528
	-1996, // 97 -0.121869
	-2280, // 98 -0.139173
	-2563, // 99 -0.156434
	-2845, // 100 -0.173648
	-3126, // 101 -0.190809
	-3406, // 102 -0.207912
	-3685, // 103 -0.224951
	-3963, // 104 -0.241922
	-4240, // 105 -0.258819
	-4516, // 106 -0.275637
	-4790, // 107 -0.292372
	-5062, // 108 -0.309017
	-5334, // 109 -0.325568
	-5603, // 110 -0.342020
	-5871, // 111 -0.358368
	-6137, // 112 -0.374607
	-6401, // 113 -0.390731
	-6663, // 114 -0.406737
	-6924, // 115 -0.422618
	-7182, // 116 -0.438371
	-7438, // 117 -0.453990
	-7691, // 118 -0.469472
	-7943, // 119 -0.484810
	-8191, // 120 -0.500000
	-8438, // 121 -0.515038
	-8682, // 122 -0.529919
	-8923, // 123 -0.544639
	-9161, // 124 -0.559193
	-9397, // 125 -0.573576
	-9630, // 126 -0.587785
	-9860, // 127 -0.601815
	-10086, // 128 -0.615661
	-10310, // 129 -0.629320
	-10531, // 130 -0.642788
	-10748, // 131 -0.656059
	-10963, // 132 -0.669131
	-11173, // 133 -0.681998
	-11381, // 134 -0.694658
	-11585, // 135 -0.707107
	-11785, // 136 -0.719340
	-11982, // 137 -0.731354
	-12175, // 138 -0.743145
	-12365, // 139 -0.754710
	-12550, // 140 -0.766044
	-12732, // 141 -0.777146
	-12910, // 142 -0.788011
	-13084, // 143 -0.798636
	-13254, // 144 -0.809017
	-13420, // 145 -0.819152
	-13582, // 146 -0.829038
	-13740, // 147 -0.838671
	-13894, // 148 -0.848048
	-14043, // 149 -0.857167
	-14188, // 150 -0.866025
	-14329, // 151 -0.874620
	-14466, // 152 -0.882948
	-14598, // 153 -0.891007
	-14725, // 154 -0.898794
	-14848, // 155 -0.906308
	-14967, // 156 -0.913545
	-15081, // 157 -0.920505
	-15190, // 158 -0.927184
	-15295, // 159 -0.933580
	-15395, // 160 -0.939693
	-15491, // 161 -0.945519
	-15582, // 162 -0.951057
	-15668, // 163 -0.956305
	-15749, // 164 -0.961262
	-15825, // 165 -0.965926
	-15897, // 166 -0.970296
	-15964, // 167 -0.974370
	-16025, // 168 -0.978148
	-16082, // 169 -0.981627
	-16135, // 170 -0.984808
	-16182, // 171 -0.987688
	-16224, // 172 -0.990268
	-16261, // 173 -0.992546
	-16294, // 174 -0.994522
	-16321, // 175 -0.996195
	-16344, // 176 -0.997564
	-16361, // 177 -0.998630
	-16374, // 178 -0.999391
	-16381, // 179 -0.999848
	-16384, // 180 -1.000000
	-16381, // 181 -0.999848
	-16374, // 182 -0.999391
	-16361, // 183 -0.998630
	-16344, // 184 -0.997564
	-16321, // 185 -0.996195
	-16294, // 186 -0.994522
	-16261, // 187 -0.992546
	-16224, // 188 -0.990268
	-16182, // 189 -0.987688
	-16135, // 190 -0.984808
	-16082, // 191 -0.981627
	-16025, // 192 -0.978148
	-15964, // 193 -0.974370
	-15897, // 194 -0.970296
	-15825, // 195 -0.965926
	-15749, // 196 -0.961262
	-15668, // 197 -0.956305
	-15582, // 198 -0.951057
	-15491, // 199 -0.945519
	-15395, // 200 -0.939693
	-15295, // 201 -0.933580
	-15190, // 202 -0.927184
	-15081, // 203 -0.920505
	-14967, // 204 -0.913545
	-14848, // 205 -0.906308
	-14725, // 206 -0.898794
	-14598, // 207 -0.891007
	-14466, // 208 -0.882948
	-14329, // 209 -0.874620
	-14188, // 210 -0.866025
	-14043, // 211 -0.857167
	-13894, // 212 -0.848048
	-13740, // 213 -0.838671
	-13582, // 214 -0.829038
	-13420, // 215 -0.819152
	-13254, // 216 -0.809017
	-13084, // 217 -0.798636
	-12910, // 218 -0.788011
	-12732, // 219 -0.777146
	-12550, // 220 -0.766044
	-12365, // 221 -0.754710
	-12175, // 222 -0.743145
	-11982, // 223 -0.731354
	-11785, // 224 -0.719340
	-11585, // 225 -0.707107
	-11381, // 226 -0.694658
	-11173, // 227 -0.681998
	-10963, // 228 -0.669131
	-10748, // 229 -0.656059
	-10531, // 230 -0.642788
	-10310, // 231 -0.629320
	-10086, // 232 -0.615661
	-9860, // 233 -0.601815
	-9630, // 234 -0.587785
	-9397, // 235 -0.573576
	-9161, // 236 -0.559193
	-8923, // 237 -0.544639
	-8682, // 238 -0.529919
	-8438, // 239 -0.515038
	-8192, // 240 -0.500000
	-7943, // 241 -0.484810
	-7691, // 242 -0.469472
	-7438, // 243 -0.453990
	-7182, // 244 -0.438371
	-6924, // 245 -0.422618
	-6663, // 246 -0.406737
	-6401, // 247 -0.390731
	-6137, // 248 -0.374607
	-5871, // 249 -0.358368
	-5603, // 250 -0.342020
	-5334, // 251 -0.325568
	-5062, // 252 -0.309017
	-4790, // 253 -0.292372
	-4516, // 254 -0.275637
	-4240, // 255 -0.258819
	-3963, // 256 -0.241922
	-3685, // 257 -0.224951
	-3406, // 258 -0.207912
	-3126, // 259 -0.190809
	-2845, // 260 -0.173648
	-2563, // 261 -0.156434
	-2280, // 262 -0.139173
	-1996, // 263 -0.121869
	-1712, // 264 -0.104528
	-1427, // 265 -0.087156
	-1142, // 266 -0.069756
	-857, // 267 -0.052336
	-571, // 268 -0.034899
	-285, // 269 -0.017452
	0, // 270 -0.000000
	285, // 271 0.017452
	571, // 272 0.034899
	857, // 273 0.052336
	1142, // 274 0.069756
	1427, // 275 0.087156
	1712, // 276 0.104528
	1996, // 277 0.121869
	2280, // 278 0.139173
	2563, // 279 0.156434
	2845, // 280 0.173648
	3126, // 281 0.190809
	3406, // 282 0.207912
	3685, // 283 0.224951
	3963, // 284 0.241922
	4240, // 285 0.258819
	4516, // 286 0.275637
	4790, // 287 0.292372
	5062, // 288 0.309017
	5334, // 289 0.325568
	5603, // 290 0.342020
	5871, // 291 0.358368
	6137, // 292 0.374607
	6401, // 293 0.390731
	6663, // 294 0.406737
	6924, // 295 0.422618
	7182, // 296 0.438371
	7438, // 297 0.453990
	7691, // 298 0.469472
	7943, // 299 0.484810
	8192, // 300 0.500000
	8438, // 301 0.515038
	8682, // 302 0.529919
	8923, // 303 0.544639
	9161, // 304 0.559193
	9397, // 305 0.573576
	9630, // 306 0.587785
	9860, // 307 0.601815
	10086, // 308 0.615661
	10310, // 309 0.629320
	10531, // 310 0.642788
	10748, // 311 0.656059
	10963, // 312 0.669131
	11173, // 313 0.681998
	11381, // 314 0.694658
	11585, // 315 0.707107
	11785, // 316 0.719340
	11982, // 317 0.731354
	12175, // 318 0.743145
	12365, // 319 0.754710
	12550, // 320 0.766044
	12732, // 321 0.777146
	12910, // 322 0.788011
	13084, // 323 0.798636
	13254, // 324 0.809017
	13420, // 325 0.819152
	13582, // 326 0.829038
	13740, // 327 0.838671
	13894, // 328 0.848048
	14043, // 329 0.857167
	14188, // 330 0.866025
	14329, // 331 0.874620
	14466, // 332 0.882948
	14598, // 333 0.891007
	14725, // 334 0.898794
	14848, // 335 0.906308
	14967, // 336 0.913545
	15081, // 337 0.920505
	15190, // 338 0.927184
	15295, // 339 0.933580
	15395, // 340 0.939693
	15491, // 341 0.945519
	15582, // 342 0.951057
	15668, // 343 0.956305
	15749, // 344 0.961262
	15825, // 345 0.965926
	15897, // 346 0.970296
	15964, // 347 0.974370
	16025, // 348 0.978148
	16082, // 349 0.981627
	16135, // 350 0.984808
	16182, // 351 0.987688
	16224, // 352 0.990268
	16261, // 353 0.992546
	16294, // 354 0.994522
	16321, // 355 0.996195
	16344, // 356 0.997564
	16361, // 357 0.998630
	16374, // 358 0.999391
	16381, // 359 0.999848
	16384 // 360 1.000000
};



https://github.com/drwonky/VGALIB/tree/master
void canvas::rotate(canvas& dest, int angle)
{
	int32_t hwidth = _width / 2;
	int32_t hheight = _height / 2;
	int32_t sinma = sindeg[angle];
	int32_t cosma = cosdeg[angle];
	int32_t xt,yt,xs,ys;
	int32_t x,y;

	for(x = 0; x < _width; x++) {
		xt = x - hwidth;
		for(y = 0; y < _height; y++) {
			yt = y - hheight;

			xs =  (cosma * xt - sinma * yt)>>14;
			xs += hwidth;
			ys =  (sinma * xt + cosma * yt)>>14;
			ys += hheight;
			if(xs >= 0 && xs < (int32_t)_width && ys >= 0 && ys < (int32_t)_height) {
				//dest._buffer[y*_width+x]=_buffer[ys*_width+xs];
				dest.setpixel(x,y,getpixel(xs,ys));
			} else {
				dest.setpixel(x,y); // @suppress("Ambiguous problem")
				//dest._buffer[y*_width+x]=_bg;
			}
		}
	}
}