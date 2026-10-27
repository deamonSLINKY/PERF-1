import React from "react";
import { Composition } from "remotion";
import { Flex } from "./Flex.jsx";

export const Root = () => (
  <Composition id="Flex" component={Flex} width={1920} height={1080} fps={30} durationInFrames={600} defaultProps={{ dir: "flex" }} />
);
