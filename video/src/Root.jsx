import React from "react";
import { Composition, staticFile } from "remotion";
import { Full, TOUR, TITLE_FRAMES } from "./Full.jsx";
import { EmuScene } from "./EmuScene.jsx";

const frames = async (id) => (await (await fetch(staticFile(`${id}/trace.json`))).json()).frames.length;

export const Root = () => (
  <>
    <Composition id="Full" component={Full} width={1920} height={1080} fps={30} durationInFrames={300} defaultProps={{ frames: 300 }}
      calculateMetadata={async () => { const n = await frames(TOUR); return { durationInFrames: TITLE_FRAMES + n, props: { frames: n } }; }} />
    <Composition id="Scene" component={EmuScene} width={1920} height={1080} fps={30} durationInFrames={600} defaultProps={{ dir: "tour" }}
      calculateMetadata={async ({ props }) => ({ durationInFrames: await frames(props.dir) })} />
  </>
);
