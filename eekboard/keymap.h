#include <gdk/gdk.h>
#include <xkbcommon/xkbcommon.h>

gboolean
squeek_keymap_get_entries_for_keyval (struct xkb_keymap     *xkb_keymap,
                                      guint          keyval,
                                      GdkKeymapKey **keys,
                                      guint          *n_keys);

static const char default_keymap[] = "xkb_keymap {\
\
xkb_keycodes \"squeekboard\" {\
        minimum = 8;\
        maximum = 255;\
        <ESC>                = 9;\
        <AE01>               = 10;\
        <AE02>               = 11;\
        <AE03>               = 12;\
        <AE04>               = 13;\
        <AE05>               = 14;\
        <AE06>               = 15;\
        <AE07>               = 16;\
        <AE08>               = 17;\
        <AE09>               = 18;\
        <AE10>               = 19;\
        <AE11>               = 20;\
        <AE12>               = 21;\
        <BKSP>               = 22;\
        <TAB>                = 23;\
        <AD01>               = 24;\
        <AD02>               = 25;\
        <AD03>               = 26;\
        <AD04>               = 27;\
        <AD05>               = 28;\
        <AD06>               = 29;\
        <AD07>               = 30;\
        <AD08>               = 31;\
        <AD09>               = 32;\
        <AD10>               = 33;\
        <AD11>               = 34;\
        <AD12>               = 35;\
        <RTRN>               = 36;\
        <LCTL>               = 37;\
        <AC01>               = 38;\
        <AC02>               = 39;\
        <AC03>               = 40;\
        <AC04>               = 41;\
        <AC05>               = 42;\
        <AC06>               = 43;\
        <AC07>               = 44;\
        <AC08>               = 45;\
        <AC09>               = 46;\
        <AC10>               = 47;\
        <AC11>               = 48;\
        <TLDE>               = 49;\
        <LFSH>               = 50;\
        <BKSL>               = 51;\
        <AB01>               = 52;\
        <AB02>               = 53;\
        <AB03>               = 54;\
        <AB04>               = 55;\
        <AB05>               = 56;\
        <AB06>               = 57;\
        <AB07>               = 58;\
        <AB08>               = 59;\
        <AB09>               = 60;\
        <AB10>               = 61;\
        <RTSH>               = 62;\
        <KPMU>               = 63;\
        <LALT>               = 64;\
        <SPCE>               = 65;\
        <CAPS>               = 66;\
        <FK01>               = 67;\
        <FK02>               = 68;\
        <FK03>               = 69;\
        <FK04>               = 70;\
        <FK05>               = 71;\
        <FK06>               = 72;\
        <FK07>               = 73;\
        <FK08>               = 74;\
        <FK09>               = 75;\
        <FK10>               = 76;\
        <NMLK>               = 77;\
        <SCLK>               = 78;\
        <KP7>                = 79;\
        <KP8>                = 80;\
        <KP9>                = 81;\
        <KPSU>               = 82;\
        <KP4>                = 83;\
        <KP5>                = 84;\
        <KP6>                = 85;\
        <KPAD>               = 86;\
        <KP1>                = 87;\
        <KP2>                = 88;\
        <KP3>                = 89;\
        <KP0>                = 90;\
        <KPDL>               = 91;\
        <LVL3>               = 92;\
        <LSGT>               = 94;\
        <FK11>               = 95;\
        <FK12>               = 96;\
        <AB11>               = 97;\
        <KATA>               = 98;\
        <HIRA>               = 99;\
        <HENK>               = 100;\
        <HKTG>               = 101;\
        <MUHE>               = 102;\
        <JPCM>               = 103;\
        <KPEN>               = 104;\
        <RCTL>               = 105;\
        <KPDV>               = 106;\
        <PRSC>               = 107;\
        <RALT>               = 108;\
        <LNFD>               = 109;\
        <HOME>               = 110;\
        <UP>                 = 111;\
        <PGUP>               = 112;\
        <LEFT>               = 113;\
        <RGHT>               = 114;\
        <END>                = 115;\
        <DOWN>               = 116;\
        <PGDN>               = 117;\
        <INS>                = 118;\
        <DELE>               = 119;\
        <I120>               = 120;\
        <MUTE>               = 121;\
        <VOL->               = 122;\
        <VOL+>               = 123;\
        <POWR>               = 124;\
        <KPEQ>               = 125;\
        <I126>               = 126;\
        <PAUS>               = 127;\
        <I128>               = 128;\
        <I129>               = 129;\
        <HNGL>               = 130;\
        <HJCV>               = 131;\
        <AE13>               = 132;\
        <LWIN>               = 133;\
        <RWIN>               = 134;\
        <COMP>               = 135;\
        <STOP>               = 136;\
        <AGAI>               = 137;\
        <PROP>               = 138;\
        <UNDO>               = 139;\
        <FRNT>               = 140;\
        <COPY>               = 141;\
        <OPEN>               = 142;\
        <PAST>               = 143;\
        <FIND>               = 144;\
        <CUT>                = 145;\
        <HELP>               = 146;\
        <I147>               = 147;\
        <I148>               = 148;\
        <I149>               = 149;\
        <I150>               = 150;\
        <I151>               = 151;\
        <I152>               = 152;\
        <I153>               = 153;\
        <I154>               = 154;\
        <I155>               = 155;\
        <I156>               = 156;\
        <I157>               = 157;\
        <I158>               = 158;\
        <I159>               = 159;\
        <I160>               = 160;\
        <I161>               = 161;\
        <I162>               = 162;\
        <I163>               = 163;\
        <I164>               = 164;\
        <I165>               = 165;\
        <I166>               = 166;\
        <I167>               = 167;\
        <I168>               = 168;\
        <I169>               = 169;\
        <I170>               = 170;\
        <I171>               = 171;\
        <I172>               = 172;\
        <I173>               = 173;\
        <I174>               = 174;\
        <I175>               = 175;\
        <I176>               = 176;\
        <I177>               = 177;\
        <I178>               = 178;\
        <I179>               = 179;\
        <I180>               = 180;\
        <I181>               = 181;\
        <I182>               = 182;\
        <I183>               = 183;\
        <I184>               = 184;\
        <I185>               = 185;\
        <I186>               = 186;\
        <I187>               = 187;\
        <I188>               = 188;\
        <I189>               = 189;\
        <I190>               = 190;\
        <FK13>               = 191;\
        <FK14>               = 192;\
        <FK15>               = 193;\
        <FK16>               = 194;\
        <FK17>               = 195;\
        <FK18>               = 196;\
        <FK19>               = 197;\
        <FK20>               = 198;\
        <FK21>               = 199;\
        <FK22>               = 200;\
        <FK23>               = 201;\
        <FK24>               = 202;\
        <MDSW>               = 203;\
        <ALT>                = 204;\
        <META>               = 205;\
        <SUPR>               = 206;\
        <HYPR>               = 207;\
        <I208>               = 208;\
        <I209>               = 209;\
        <I210>               = 210;\
        <I211>               = 211;\
        <I212>               = 212;\
        <I213>               = 213;\
        <I214>               = 214;\
        <I215>               = 215;\
        <I216>               = 216;\
        <I217>               = 217;\
        <I218>               = 218;\
        <I219>               = 219;\
        <I220>               = 220;\
        <I221>               = 221;\
        <I222>               = 222;\
        <I223>               = 223;\
        <I224>               = 224;\
        <I225>               = 225;\
        <I226>               = 226;\
        <I227>               = 227;\
        <I228>               = 228;\
        <I229>               = 229;\
        <I230>               = 230;\
        <I231>               = 231;\
        <I232>               = 232;\
        <I233>               = 233;\
        <I234>               = 234;\
        <I235>               = 235;\
        <I236>               = 236;\
        <I237>               = 237;\
        <I238>               = 238;\
        <I239>               = 239;\
        <I240>               = 240;\
        <I241>               = 241;\
        <I242>               = 242;\
        <I243>               = 243;\
        <I244>               = 244;\
        <I245>               = 245;\
        <I246>               = 246;\
        <I247>               = 247;\
        <I248>               = 248;\
        <I249>               = 249;\
        <I250>               = 250;\
        <I251>               = 251;\
        <I252>               = 252;\
        <I253>               = 253;\
        <I254>               = 254;\
        <I255>               = 255;\
    };\
\
    xkb_symbols \"squeekboard\" {\
\
        name[Group1] = \"Letters\";\
        name[Group2] = \"Numbers/Symbols\";\
\
        key <AD01> { [ q, Q ], [ 1, asciitilde ] };\
        key <AD02> { [ w, W ], [ 2, quoteleft ] };\
        key <AD03> { [ e, E ], [ 3, bar ] };\
        key <AD04> { [ r, R ], [ 4, U00B7 ] };\
        key <AD05> { [ t, T ], [ 5, squareroot ] };\
        key <AD06> { [ y, Y ], [ 6, Greek_pi ] };\
        key <AD07> { [ u, U ], [ 7, division ] };\
        key <AD08> { [ i, I ], [ 8, multiply ] };\
        key <AD09> { [ o, O ], [ 9, paragraph ] };\
        key <AD10> { [ p, P ], [ 0, U25B3 ] };\
        key <AD11> { [ aring, Aring ], [ U00B1, U00A7 ] };\
        key <AC01> { [ a, A ], [ at, copyright ] };\
        key <AC02> { [ s, S ], [ numbersign, U00AE ] };\
        key <AC03> { [ d, D ], [ dollar, U00A3 ] };\
        key <AC04> { [ f, F ], [ percent, EuroSign ] };\
        key <AC05> { [ g, G ], [ ampersand, U00A5 ] };\
        key <AC06> { [ h, H ], [ minus, underscore ] };\
        key <AC07> { [ j, J ], [ plus, equal ] };\
        key <AC08> { [ k, K ], [ U00FC, asciicircum ] };\
        key <AC09> { [ l, L ], [ U00F6, degree ] };\
        key <AC10> { [ oslash, Oslash ], [ parenleft, braceleft ] };\
        key <AC11> { [ ae, AE ], [ parenright, braceright ] };\
        key <RTRN> { [ Return, Return ], [ Return, Return ] };\
        key <LFSH> { [ Shift_L, Shift_L ], [ Shift_L, Shift_L ] };\
        key <AB01> { [ z, Z ], [ comma, backslash ] };\
        key <AB02> { [ x, X ], [ quotedbl, slash ] };\
        key <AB03> { [ c, C ], [ quoteright, less ] };\
        key <AB04> { [ v, V ], [ colon, greater ] };\
        key <AB05> { [ b, B ], [ semicolon, equal ] };\
        key <AB06> { [ n, N ], [ exclam, bracketleft ] };\
        key <AB07> { [ m, M ], [ question, bracketright ] };\
        key <AB08> { [ period, period ], [ period, period ] };\
        key <I149> { [ preferences, preferences ], [ preferences, preferences ] };\
        key <SPCE> { [ space, space ], [ space, space ] };\
        key <BKSP> { [ Backspace, Backspace ], [ Backspace, Backspace ] };\
    };\
\
    xkb_types \"squeekboard\" {\
\
	type \"TWO_LEVEL\" {\
		modifiers = Shift;\
		map[Shift] = Level2;\
		level_name[Level1] = \"Base\";\
		level_name[Level2] = \"Shift\";\
	};\
    };\
\
    xkb_compatibility \"squeekboard\" {\
    };\
};";
