// The whole video: the title, then the tour (one trace: one take of the device).
import React from "react";
import { Series } from "remotion";
import { EmuScene } from "./EmuScene.jsx";
import { Title } from "./Cards.jsx";

export const TITLE_FRAMES = 135;
export const TOUR = "tour";

export function Full({ frames }) {
  return (
    <Series>
      <Series.Sequence durationInFrames={TITLE_FRAMES}><Title /></Series.Sequence>
      <Series.Sequence durationInFrames={frames}><EmuScene dir={TOUR} /></Series.Sequence>
    </Series>
  );
}
