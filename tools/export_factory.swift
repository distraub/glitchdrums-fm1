import Foundation
@main struct ExportFactory {
 static func main() throws {
  let models: [DrumPadModel] = [.kick,.snare,.closedHat,.openHat,.clap,.percussion,.glitch,.analog]
  let rows:[[String:Any]]=models.flatMap { model in
   FactoryVoicePresetCatalog.presets(for:model).prefix(8).enumerated().map { index,f in
    ["model":String(describing:model),"slot":index+1,"name":f.preset.name,"level":f.preset.level,
     "parameters":Dictionary(uniqueKeysWithValues:zip(VoiceEditParameterCatalog.parameters.map{String(describing:$0)},f.preset.values))]
   }
  }
  try JSONSerialization.data(withJSONObject:rows,options:[.prettyPrinted,.sortedKeys]).write(to:URL(fileURLWithPath:CommandLine.arguments[1]))
 }
}
