import React from "react";
import { Composition, staticFile } from "remotion";
import { Full, IDS, CREDITS, TITLE_FRAMES } from "./Full.jsx";
import { EmuScene } from "./EmuScene.jsx";

const frames = async (id) => (await (await fetch(staticFile(`${id}/trace.json`))).json()).frames.length;

export const Root = () => (
  <>
    <Composition id="Full" component={Full} width={1920} height={1080} fps={30} durationInFrames={300} defaultProps={{ lens: IDS.concat(CREDITS).map(() => 300) }}
      calculateMetadata={async () => {
        const lens = await Promise.all(IDS.concat(CREDITS).map(frames));
        return { durationInFrames: TITLE_FRAMES + lens.reduce((a, b) => a + b, 0), props: { lens } };
      }} />
    <Composition id="Scene" component={EmuScene} width={1920} height={1080} fps={30} durationInFrames={600} defaultProps={{ dir: "s7_flex" }}
      calculateMetadata={async ({ props }) => ({ durationInFrames: await frames(props.dir) })} />
  </>
);
