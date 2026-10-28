// The whole video: the title, the scenes in order (each from its own trace), the credits.
import React from "react";
import { Series } from "remotion";
import { EmuScene } from "./EmuScene.jsx";
import { Title, Credits } from "./Cards.jsx";

export const TITLE_FRAMES = 135;
export const IDS = ["s1_poweron", "s2_perform", "s3_autodj", "s4_master", "s5_fxedit", "s6_beatfx", "s7_flex", "s8_genres", "s9_more"];
export const CREDITS = "s10_credits";

export function Full({ lens }) {
  return (
    <Series>
      <Series.Sequence durationInFrames={TITLE_FRAMES}><Title /></Series.Sequence>
      {IDS.map((id, i) => <Series.Sequence key={id} durationInFrames={lens[i]}><EmuScene dir={id} /></Series.Sequence>)}
      <Series.Sequence durationInFrames={lens[IDS.length]}><Credits dir={CREDITS} /></Series.Sequence>
    </Series>
  );
}
