#include "Simulation/LRSimulation.h"

#include "Simulation/LRHexGrid.h"
#include "Simulation/LRPhysics.h"

namespace
{
	const FName ReasonUnknownAction(TEXT("unknown_action"));
	const FName ReasonNotRevealed(TEXT("not_revealed"));
	const FName ReasonCellBusy(TEXT("cell_busy"));
	const FName ReasonOnCooldown(TEXT("on_cooldown"));
	const FName ReasonRequirements(TEXT("requirements_not_met"));
	const FName ReasonUnknownRecipe(TEXT("unknown_recipe"));
	const FName ReasonRecipeLocked(TEXT("recipe_locked"));
	const FName ReasonInsufficientMaterials(TEXT("insufficient_materials"));
	const FName ReasonNoLootBox(TEXT("no_loot_box"));
	const FName ReasonNoLootTable(TEXT("no_loot_table"));
	const FName ReasonNoCell(TEXT("no_cell"));
	const FName ReasonOccupied(TEXT("occupied"));
	const FName ReasonNothingPlaced(TEXT("no_placed_entity"));
	const FName ReasonNeedsIrradiator(TEXT("needs_irradiator"));
	const FName ReasonChamberFull(TEXT("chamber_full"));
	const FName ReasonSourceFull(TEXT("source_full"));
	const FName ReasonTooStrong(TEXT("radiation_too_strong"));
	const FName ReasonCantDismantle(TEXT("cannot_dismantle"));
	const FName ReasonRippleAtMax(TEXT("ripple_at_max"));
	const FName ReasonHorizonWeak(TEXT("horizon_too_weak"));
	const FName ReasonNoHost(TEXT("no_host"));
	const FName ReasonInstrumentsDown(TEXT("instruments_down"));
	const FName ReasonNoBuilds(TEXT("no_builds"));
	const FName ReasonGameOver(TEXT("game_over"));

	/** The universe's temperature a second after the Big Bang, K (radiation era: T ~ 1 / sqrt(t)). */
	constexpr double CosmicRadiationTemperature = 1.5e10;
	/** Matter-radiation equality, s (about 50,000 years): after it, T ~ t^(-2/3). */
	constexpr double CosmicEqualitySeconds = 1.6e12;

	/** Seconds between the steps of the alarm's forecast. */
	constexpr double AlarmStepSeconds = 1.0;

	/** "45 s", "4 min 50 s" for the alarms. */
	FString FormatAlarmTime(double Seconds)
	{
		const int32 Whole = FMath::Max(0, FMath::CeilToInt32(Seconds));
		return Whole < 60 ? FString::Printf(TEXT("%d s"), Whole) : FString::Printf(TEXT("%d min %02d s"), Whole / 60, Whole % 60);
	}

	/** "4.9 teratons of TNT", "12 kilotons of TNT". */
	FString FormatTntEquivalent(double Joules)
	{
		const double Tons = Joules / 4.184e9;
		const TCHAR* Units[] = { TEXT("tons"), TEXT("kilotons"), TEXT("megatons"), TEXT("gigatons"), TEXT("teratons") };
		int32 Unit = 0;
		double Amount = Tons;
		while (Amount >= 1000.0 && Unit < 4)
		{
			Amount /= 1000.0;
			++Unit;
		}
		return FString::Printf(Amount < 10.0 ? TEXT("%.1f %s of TNT") : TEXT("%.0f %s of TNT"), Amount, Units[Unit]);
	}

	/** A fixed order for the two cells of a grid edge, so each edge has one key. */
	bool IsGasEdgeFirst(const FIntVector& A, const FIntVector& B)
	{
		if (A.X != B.X) { return A.X < B.X; }
		if (A.Y != B.Y) { return A.Y < B.Y; }
		return A.Z < B.Z;
	}

	const FName IrradiateEvent(TEXT("irradiate"));
	const FName UnlockEvent(TEXT("unlock"));
	const FName HostEvent(TEXT("host"));
}

FString FLRSimulation::DescribeReasonShort(FName Reason)
{
	if (Reason.IsNone() || Reason == ReasonOnCooldown) { return FString(); } // a moment's pause, not worth a word
	if (Reason == ReasonCellBusy) { return TEXT("busy"); }
	if (Reason == ReasonInstrumentsDown) { return TEXT("instruments down"); }
	if (Reason == ReasonRequirements || Reason == ReasonRecipeLocked || Reason == ReasonNotRevealed) { return TEXT("locked"); }
	if (Reason == ReasonInsufficientMaterials) { return TEXT("not enough matter"); }
	if (Reason == ReasonNoLootBox) { return TEXT("no cache"); }
	if (Reason == ReasonOccupied) { return TEXT("occupied"); }
	if (Reason == ReasonNothingPlaced) { return TEXT("empty"); }
	if (Reason == ReasonNeedsIrradiator) { return TEXT("needs an irradiator"); }
	if (Reason == ReasonChamberFull) { return TEXT("chamber full"); }
	if (Reason == ReasonSourceFull) { return TEXT("has a source"); }
	if (Reason == ReasonTooStrong) { return TEXT("too strong"); }
	if (Reason == ReasonCantDismantle) { return TEXT("can't"); }
	if (Reason == ReasonRippleAtMax) { return TEXT("at max"); }
	if (Reason == ReasonHorizonWeak) { return TEXT("host too small"); }
	if (Reason == ReasonNoHost) { return TEXT("no host"); }
	if (Reason == ReasonNoBuilds) { return TEXT("no builds"); }
	if (Reason == ReasonGameOver) { return TEXT("game over"); }
	return TEXT("unavailable");
}

FString FLRSimulation::DescribeReason(FName Reason)
{
	if (Reason == ReasonCellBusy) { return TEXT("something is already under way in that cell"); }
	if (Reason == ReasonOnCooldown) { return TEXT("on cooldown"); }
	if (Reason == ReasonRequirements) { return TEXT("requirements not met"); }
	if (Reason == ReasonUnknownRecipe) { return TEXT("pick something to build"); }
	if (Reason == ReasonRecipeLocked) { return TEXT("that recipe isn't unlocked yet"); }
	if (Reason == ReasonInsufficientMaterials) { return TEXT("not enough matter within reach"); }
	if (Reason == ReasonNoLootBox) { return TEXT("there's no cache in that cell"); }
	if (Reason == ReasonNoCell) { return TEXT("select a grid cell first"); }
	if (Reason == ReasonOccupied) { return TEXT("that cell is occupied"); }
	if (Reason == ReasonNothingPlaced) { return TEXT("there's nothing to dismantle in that cell"); }
	if (Reason == ReasonNeedsIrradiator) { return TEXT("sources are built into an irradiator"); }
	if (Reason == ReasonChamberFull) { return TEXT("the irradiator already holds a cache"); }
	if (Reason == ReasonSourceFull) { return TEXT("the irradiator already holds a source"); }
	if (Reason == ReasonTooStrong) { return TEXT("this irradiator can't contain radiation that strong"); }
	if (Reason == ReasonCantDismantle) { return TEXT("that is part of the pocket universe now"); }
	if (Reason == ReasonRippleAtMax) { return TEXT("that ripple can't get any deeper"); }
	if (Reason == ReasonHorizonWeak) { return TEXT("the host black hole can't spare that much mass, feed it first"); }
	if (Reason == ReasonNoHost) { return TEXT("there is no host black hole"); }
	if (Reason == ReasonInstrumentsDown) { return TEXT("the instruments are down"); }
	if (Reason == ReasonNoBuilds) { return TEXT("nothing can be built there"); }
	if (Reason == ReasonGameOver) { return TEXT("the game is over"); }
	return Reason.ToString();
}

// ---------------------------------------------------------------------------------------
// Transaction
// ---------------------------------------------------------------------------------------

FLRSimulation::FTransaction::FTransaction(FLRSimulation& InSim)
	: Sim(InSim)
	, Matter(InSim.Matter)
	, LootBoxes(InSim.LootBoxes)
	, Placed(InSim.Placed)
	, NextInstanceId(InSim.NextInstanceId)
{
}

FLRSimulation::FTransaction::~FTransaction()
{
	if (!bCommitted)
	{
		Sim.Matter = MoveTemp(Matter);
		Sim.LootBoxes = MoveTemp(LootBoxes);
		Sim.Placed = MoveTemp(Placed);
		Sim.NextInstanceId = NextInstanceId;
	}
}

// ---------------------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------------------

FLRSimulation::FLRSimulation(const FLRGameData& InData, int32 Seed)
	: Data(InData)
{
	Reset(Seed);
}

void FLRSimulation::Reset(int32 Seed)
{
	Now = 0.0;
	Rng.Initialize(Seed);
	NextInstanceId = 1;
	Matter.Reset();
	LootBoxes.Reset();
	Placed.Reset();
	ActionStates.Reset();
	Jobs.Reset();
	GasCarry.Reset();
	GasProgress = 0.0;
	GlobalCooldownUntil = 0.0;
	Unlocked.Reset();
	Stats.Reset();
	HostMass = Data.Host.StartMass;
	InjectorTarget = 0.0;
	InjectorAuto = ELRInjectorAuto::Off;
	bVenting = false;
	Injector = FLRInjectorState();
	bBreached = false;
	Announced = FLRHostAlarm();
	RefreshAlarm(/*bAnnounce*/ false);
	CosmicTime = 0.0;
	EnterEpoch(0);
	RefreshUnlocks(); // starting unlocks, not announced

	OnMatterChanged.Broadcast();
	OnWorldChanged.Broadcast();
}

void FLRSimulation::EnterEpoch(int32 Index)
{
	EpochIndex = Data.Epochs.IsValidIndex(Index) ? Index : 0;
	EpochStartedAt = Now;
	if (const FLREpochDef* Epoch = GetEpoch())
	{
		CosmicTime = FMath::Max(CosmicTime, Epoch->StartTime);
	}
}

FLRSaveData FLRSimulation::Save() const
{
	FLRSaveData Out;
	Out.Version = SaveVersion;
	Out.Now = Now;
	Out.RandomSeed = Rng.GetCurrentSeed();
	Out.NextInstanceId = NextInstanceId;
	Matter.GenerateValueArray(Out.Matter);
	LootBoxes.GenerateValueArray(Out.LootBoxes);
	Placed.GenerateValueArray(Out.Placed);
	ActionStates.GenerateValueArray(Out.Actions);
	Jobs.GenerateValueArray(Out.Jobs);
	Out.Unlocked = Unlocked.Array();
	Out.Stats = Stats;
	const FLREpochDef* Epoch = GetEpoch();
	Out.Epoch = Epoch ? Epoch->Id : NAME_None;
	Out.EpochStartedAt = EpochStartedAt;
	Out.CosmicTime = CosmicTime;
	Out.HostMass = HostMass;
	Out.InjectorTarget = InjectorTarget;
	Out.InjectorAuto = InjectorAuto;
	Out.bVenting = bVenting;
	Out.InjectorFlow = Injector.Flow;
	Out.InjectorChange = Injector.Change;
	Out.bBreached = bBreached;
	return Out;
}

bool FLRSimulation::Load(const FLRSaveData& SaveData)
{
	if (SaveData.Version < MinCompatibleSaveVersion || SaveData.Version > SaveVersion)
	{
		return false;
	}

	Now = SaveData.Now;
	Rng.Initialize(SaveData.RandomSeed);
	NextInstanceId = FMath::Max(1, SaveData.NextInstanceId);

	Matter.Reset();
	for (const FLRCellMatter& CellMatter : SaveData.Matter)
	{
		if (!CellMatter.IsEmpty())
		{
			Matter.Add(CellMatter.Cell, CellMatter);
		}
	}

	LootBoxes.Reset();
	for (const FLRLootBoxInstance& Box : SaveData.LootBoxes)
	{
		LootBoxes.Add(Box.InstanceId, Box);
	}
	Placed.Reset();
	for (const FLRPlacedEntity& Entity : SaveData.Placed)
	{
		Placed.Add(Entity.Cell, Entity);
	}
	ActionStates.Reset();
	for (const FLRActionState& State : SaveData.Actions)
	{
		ActionStates.Add(State.Name, State);
	}
	GasCarry.Reset();
	GasProgress = 0.0;
	Jobs.Reset();
	for (const FLRCellJob& Job : SaveData.Jobs)
	{
		if (Job.Request.bHasCell)
		{
			Jobs.Add(Job.Request.Cell, Job);
		}
	}
	GlobalCooldownUntil = 0.0;

	Unlocked.Reset();
	Unlocked.Append(SaveData.Unlocked);
	Stats = SaveData.Stats;
	HostMass = Data.Host.IsDefined() ? FMath::Max(0.0, SaveData.HostMass) : 0.0;
	InjectorTarget = FMath::Clamp(SaveData.InjectorTarget, 0.0, Data.Host.InjectorMaxRate);
	InjectorAuto = SaveData.InjectorAuto;
	bVenting = SaveData.bVenting && Data.Host.IsDefined();
	Injector.Flow = FMath::Clamp(SaveData.InjectorFlow, -Data.Host.InjectorMaxRate, Data.Host.InjectorMaxRate);
	Injector.Change = SaveData.InjectorChange;
	bBreached = SaveData.bBreached && Data.Host.IsDefined();
	Announced = FLRHostAlarm(); // the log announces any alarm the loaded game is in
	RefreshAlarm(/*bAnnounce*/ false);
	CosmicTime = SaveData.CosmicTime;
	EnterEpoch(FMath::Max(0, Data.FindEpochIndex(SaveData.Epoch)));
	EpochStartedAt = SaveData.EpochStartedAt; // keep an in-progress plasma fade going
	RefreshUnlocks(); // catch up with data changes, not announced

	// Never hand out an instance id that is already in use, even if the save is inconsistent.
	for (const TPair<int32, FLRLootBoxInstance>& Pair : LootBoxes)
	{
		NextInstanceId = FMath::Max(NextInstanceId, Pair.Key + 1);
	}
	for (const TPair<FIntVector, FLRPlacedEntity>& Pair : Placed)
	{
		NextInstanceId = FMath::Max(NextInstanceId, Pair.Value.InstanceId + 1);
		NextInstanceId = FMath::Max(NextInstanceId, Pair.Value.Chamber.InstanceId + 1);
		NextInstanceId = FMath::Max(NextInstanceId, Pair.Value.Source.InstanceId + 1);
	}

	OnMatterChanged.Broadcast();
	OnWorldChanged.Broadcast();
	return true;
}

// ---------------------------------------------------------------------------------------
// Clock
// ---------------------------------------------------------------------------------------

void FLRSimulation::Advance(double DeltaSeconds)
{
	if (DeltaSeconds <= 0.0 || IsFrozen())
	{
		return; // once the game is over, nothing moves
	}
	Now += DeltaSeconds;

	// Jobs that have run their time are carried out, in the order they end. Rails: Solid Queue
	// running PerformPlayerActionJob once its `wait:` elapsed.
	TArray<FLRCellJob> Due;
	for (const TPair<FIntVector, FLRCellJob>& Pair : Jobs)
	{
		if (Pair.Value.EndsAt <= Now)
		{
			Due.Add(Pair.Value);
		}
	}
	Due.Sort([](const FLRCellJob& A, const FLRCellJob& B) { return A.EndsAt < B.EndsAt; });
	for (const FLRCellJob& Job : Due)
	{
		Jobs.Remove(Job.Request.Cell); // the cell is free again before the action runs
		FLRActionResult Result = Execute(Job.Request, /*bPrepaid*/ true);
		if (!Result.bSuccess && !Job.Paid.IsEmpty())
		{
			// It paid when it started, and it failed through no choice of the player's: put
			// everything back where it came from.
			Result.Message += DescribeRefund(RefundTransaction(Job.Paid));
		}
		Complete(Result);
	}

	AdvanceHost(DeltaSeconds);
	AdvanceCosmos(DeltaSeconds);
	if (!IsFrozen())
	{
		AdvanceIrradiation(DeltaSeconds);
		AdvanceStructures(DeltaSeconds);
		AdvanceGas(DeltaSeconds);
	}
}

void FLRSimulation::AdvanceHost(double DeltaSeconds)
{
	const FLRHostDef& Host = Data.Host;
	if (!Host.IsDefined() || IsFrozen())
	{
		return;
	}
	const int32 TonnesBefore = FMath::FloorToInt32(HostMass / 1000.0);
	const double Cap = Host.GetContainmentCap();
	TArray<FString> Messages;

	// Feeding and evaporation are coupled (the rated limit and the loss both depend on the
	// mass), so step in small slices. Each slice is exact for evaporation and the injectors.
	FHostState State = GetHostState();
	double Remaining = DeltaSeconds;
	while (Remaining > 0.0)
	{
		const double Step = FMath::Min(Remaining, HostStepSeconds);
		Remaining -= Step;
		StepHostState(Host, State, Step);
		if (State.Mass <= 0.0)
		{
			State.Mass = 0.0;
			Messages.Add(TEXT("THE HOST HAS EVAPORATED. Past its point of no return it burned hotter and faster, and went off in a final flash. The facility is gone with it."));
			break;
		}
		// No safeties: once the 1 g sphere reaches the chamber wall, the containment fails.
		if (Cap > 0.0 && State.Mass >= Cap)
		{
			bBreached = true;
			Messages.Add(FString::Printf(TEXT("CONTAINMENT BREACH: the host's gravity well reached the chamber wall at %s, and the containment field failed."),
				*FormatMass(State.Mass)));
			break;
		}
	}
	HostMass = State.Mass;
	Injector = State.Injector;
	InjectorTarget = State.Target;
	if (IsFrozen())
	{
		Injector = FLRInjectorState();
		bVenting = false;
		Alarm = FLRHostAlarm();
	}
	else
	{
		RefreshAlarm(/*bAnnounce*/ true);
	}

	AnnounceEvents(HostEvent, Messages);
	// "host" requirements count whole tonnes, so only re-check when that changes.
	if (!IsFrozen() && FMath::FloorToInt32(HostMass / 1000.0) != TonnesBefore)
	{
		AnnounceUnlocks(RefreshUnlocks());
	}
	if (IsFrozen())
	{
		OnWorldChanged.Broadcast();
	}
}

FLRSimulation::FHostState FLRSimulation::GetHostState() const
{
	FHostState State;
	State.Mass = HostMass;
	State.Injector = Injector;
	State.Target = InjectorTarget;
	State.Auto = InjectorAuto;
	State.bVenting = bVenting;
	return State;
}

void FLRSimulation::StepHostState(const FLRHostDef& Host, FHostState& State, double Step)
{
	if (State.Mass <= 0.0)
	{
		return;
	}
	// An auto mode keeps the dial on its mark for the mass now.
	if (State.Auto != ELRInjectorAuto::Off && !State.bVenting)
	{
		const double Mark = State.Auto == ELRInjectorAuto::Hold ? Host.GetEvaporationRate(State.Mass) : Host.GetRatedLimit(State.Mass);
		State.Target = FMath::Clamp(Mark, 0.0, FMath::Max(0.0, Host.InjectorMaxRate));
	}
	// Venting runs the same injectors in reverse: the dial sets how hard they pull.
	State.Injector = StepInjector(State.Injector, State.bVenting ? -State.Target : State.Target, Step, Host.InjectorResponseSeconds);
	if (FMath::Abs(State.Injector.Flow) > Host.InjectorMaxRate)
	{
		State.Injector.Flow = FMath::Sign(State.Injector.Flow) * Host.InjectorMaxRate;
		State.Injector.Change = 0.0;
	}
	if (!State.bVenting && State.Injector.Flow < 0.0 && State.Injector.Flow > -VentResidualRate)
	{
		State.Injector = FLRInjectorState(); // wound down after venting
	}

	// Hawking evaporation, exact over the slice: dM/dt = -K / M^2, so M^3 falls by 3K per second.
	const double K = Host.GetEvaporationConstant();
	if (K > 0.0)
	{
		const double Cubed = State.Mass * State.Mass * State.Mass - 3.0 * K * Step;
		State.Mass = Cubed > 0.0 ? FMath::Pow(Cubed, 1.0 / 3.0) : 0.0;
		if (State.Mass <= 0.0)
		{
			return;
		}
	}

	// Feeding: the flow reaches the host up to the rated limit; the rest is blown back out.
	// Venting pulls no harder than that either.
	const double Limit = Host.GetRatedLimit(State.Mass);
	const double Intake = State.Injector.Flow > 0.0 ? FMath::Min(State.Injector.Flow, Limit) : 0.0;
	const double Extraction = State.Injector.Flow < 0.0 ? FMath::Min(-State.Injector.Flow, Limit) : 0.0;
	State.Mass = FMath::Max(0.0, State.Mass + (Intake - Extraction) * Step);
}

void FLRSimulation::RefreshAlarm(bool bAnnounce)
{
	FLRHostAlarm Next;
	const FLRHostDef& Host = Data.Host;
	if (Host.IsDefined() && !IsFrozen() && HostMass > 0.0)
	{
		const double Tipping = Host.GetTippingMass();
		const double Cap = Host.GetContainmentCap();
		if (Tipping > 0.0 && HostMass < Tipping)
		{
			// Past the point of no return: it's only a matter of time.
			Next.Kind = ELRAlarmKind::Evaporating;
			Next.Seconds = GetTimeToEvaporation();
		}
		else
		{
			// Run the host forward as the dial is set now, to the first disaster within the
			// caution horizon.
			FHostState State = GetHostState();
			double Elapsed = 0.0;
			while (Elapsed < static_cast<double>(Host.CautionSeconds))
			{
				StepHostState(Host, State, AlarmStepSeconds);
				Elapsed += AlarmStepSeconds;
				if (Cap > 0.0 && State.Mass >= Cap)
				{
					Next.Kind = ELRAlarmKind::Breach;
					break;
				}
				if (Tipping > 0.0 && State.Mass < Tipping)
				{
					Next.Kind = ELRAlarmKind::NoReturn;
					break;
				}
			}
			Next.Seconds = Elapsed;
		}
		if (Next.Kind != ELRAlarmKind::None)
		{
			Next.Level = (Next.Kind == ELRAlarmKind::Evaporating || Next.Seconds <= Host.CriticalSeconds) ? ELRAlarmLevel::Critical
				: (Next.Seconds <= Host.WarningSeconds ? ELRAlarmLevel::Warning : ELRAlarmLevel::Caution);
		}
		else
		{
			Next.Seconds = 0.0;
		}
	}
	Alarm = Next;
	if (!bAnnounce)
	{
		return;
	}

	// The log: once per step up (or a different disaster), and once when it's all clear.
	const bool bWorse = Next.Level > Announced.Level || (Next.IsActive() && Next.Kind != Announced.Kind);
	if (bWorse)
	{
		const TCHAR* Level = Next.Level == ELRAlarmLevel::Critical ? TEXT("CRITICAL") : (Next.Level == ELRAlarmLevel::Warning ? TEXT("WARNING") : TEXT("CAUTION"));
		FString Message;
		switch (Next.Kind)
		{
		case ELRAlarmKind::Breach:
			Message = FString::Printf(TEXT("%s: at this setting the host's gravity well reaches the chamber wall (%s) in %s. There are no safeties: turn the dial down."),
				Level, *FormatMass(Host.GetContainmentCap()), *FormatAlarmTime(Next.Seconds));
			break;
		case ELRAlarmKind::NoReturn:
			Message = FString::Printf(TEXT("%s: at this setting the host falls past its point of no return (%s) in %s. Feed it: open the dial."),
				Level, *FormatMass(Host.GetTippingMass()), *FormatAlarmTime(Next.Seconds));
			break;
		case ELRAlarmKind::Evaporating:
			Message = FString::Printf(TEXT("PAST THE POINT OF NO RETURN: evaporation outruns anything the injectors can feed. The host goes off in about %s."),
				*FormatAlarmTime(Next.Seconds));
			break;
		default:
			break;
		}
		AnnounceEvents(HostEvent, { Message });
	}
	else if (!Next.IsActive() && Announced.IsActive())
	{
		AnnounceEvents(HostEvent, { FString(TEXT("All clear: the host is out of danger at this setting.")) });
	}
	if (bWorse || Next.Level < Announced.Level)
	{
		Announced = Next; // stepping down lets it announce again if things get worse
	}
}

ELRGameOver FLRSimulation::GetGameOver() const
{
	if (!Data.Host.IsDefined())
	{
		return ELRGameOver::None;
	}
	if (bBreached)
	{
		return ELRGameOver::Breach;
	}
	return HostMass <= 0.0 ? ELRGameOver::Evaporated : ELRGameOver::None;
}

FString FLRSimulation::DescribeGameOver() const
{
	switch (GetGameOver())
	{
	case ELRGameOver::Breach:
		return FString::Printf(TEXT("The host's gravity well reached the chamber wall at %s, and with no safeties the containment field failed. An unshielded singularity glowing at %.1e W fell through the floor of the facility."),
			*FormatMass(HostMass), LRPhysics::HawkingPower(HostMass));
	case ELRGameOver::Evaporated:
	{
		// Its last second: the mass it had with one second to go, all turned to energy.
		const double LastMass = FMath::Pow(3.0 * Data.Host.GetEvaporationConstant(), 1.0 / 3.0);
		const double Joules = LastMass * LRPhysics::C * LRPhysics::C;
		return FString::Printf(TEXT("Past its point of no return the host evaporated, faster and hotter to the end. Its last second released %.1e J, about %s. The facility, and the pocket universe it held, are gone."),
			Joules, *FormatTntEquivalent(Joules));
	}
	default:
		return FString();
	}
}

FLRInjectorState FLRSimulation::StepInjector(const FLRInjectorState& State, double Target, double DeltaSeconds, double ResponseSeconds)
{
	if (DeltaSeconds <= 0.0)
	{
		return State;
	}
	// A critically damped spring on e = Flow - Target: e'' = -w^2 e - 2w e'. Its exact solution
	// is e(t) = (e0 + (v0 + w e0) t) exp(-wt). From rest it starts parabolically (about
	// -w^2 e0 t^2 / 2) and never overshoots. (1 + x) exp(-x) = 0.1 at x = 3.8897, so it gets 90%
	// of the way at 3.8897 / w.
	const double Omega = 3.8897 / FMath::Max(ResponseSeconds, 1e-3);
	const double Error = State.Flow - Target;
	const double Momentum = State.Change + Omega * Error;
	const double Decay = FMath::Exp(-Omega * DeltaSeconds);

	FLRInjectorState Out;
	Out.Flow = Target + (Error + Momentum * DeltaSeconds) * Decay;
	Out.Change = (State.Change - Omega * Momentum * DeltaSeconds) * Decay;
	// Never cross zero away from the target: turning the dial down doesn't run the injectors
	// backwards, but reversing them (a negative target) ramps through zero.
	if ((State.Flow >= 0.0 && Target >= 0.0 && Out.Flow < 0.0) || (State.Flow <= 0.0 && Target <= 0.0 && Out.Flow > 0.0))
	{
		Out.Flow = 0.0;
		Out.Change = 0.0;
	}
	return Out;
}

void FLRSimulation::SetInjectorTarget(double KgPerSecond)
{
	InjectorAuto = ELRInjectorAuto::Off;
	InjectorTarget = FMath::Clamp(KgPerSecond, 0.0, FMath::Max(0.0, Data.Host.InjectorMaxRate));
	RefreshAlarm(/*bAnnounce*/ false);
}

void FLRSimulation::SetInjectorAuto(ELRInjectorAuto Mode)
{
	InjectorAuto = bVenting ? ELRInjectorAuto::Off : Mode; // only the dial itself works while venting
	UpdateAutoTarget();
	RefreshAlarm(/*bAnnounce*/ false);
}

void FLRSimulation::UpdateAutoTarget()
{
	// With no host there's no mark to follow.
	if (InjectorAuto == ELRInjectorAuto::Off || bVenting || HostMass <= 0.0)
	{
		return;
	}
	const double Mark = InjectorAuto == ELRInjectorAuto::Hold ? GetEvaporationRate() : GetRatedLimit();
	InjectorTarget = FMath::Clamp(Mark, 0.0, FMath::Max(0.0, Data.Host.InjectorMaxRate));
}

double FLRSimulation::GetIntakeRate() const
{
	if (HostMass <= 0.0 || Injector.Flow <= 0.0)
	{
		return 0.0;
	}
	return FMath::Min(Injector.Flow, GetRatedLimit());
}

double FLRSimulation::GetExtractionRate() const
{
	if (HostMass <= 0.0 || Injector.Flow >= 0.0)
	{
		return 0.0;
	}
	return FMath::Min(-Injector.Flow, GetRatedLimit());
}

double FLRSimulation::GetTimeToEvaporation() const
{
	const double K = Data.Host.GetEvaporationConstant();
	if (HostMass <= 0.0)
	{
		return 0.0;
	}
	if (GetNetRate() >= 0.0)
	{
		return TNumericLimits<double>::Max();
	}
	// Venting: dM/dt = -E - K / M^2 with a steady extraction E, which takes
	// M / E - sqrt(K / E) / E * atan(M sqrt(E / K)) to reach zero (M / E with no evaporation).
	const double Extraction = GetExtractionRate();
	if (Extraction > 0.0)
	{
		if (K <= 0.0)
		{
			return HostMass / Extraction;
		}
		const double Scale = FMath::Sqrt(K / Extraction);
		return HostMass / Extraction - Scale / Extraction * FMath::Atan(HostMass / Scale);
	}
	if (K <= 0.0)
	{
		return TNumericLimits<double>::Max();
	}
	// dM/dt = I - K / M^2 with a steady intake I. Shrinking means M < a = sqrt(K / I), and the
	// time to reach zero is (a artanh(M / a) - M) / I. With little or no intake that tends to
	// the unfed lifetime, M^3 / 3K, which is also the numerically safe form there.
	const double Intake = GetIntakeRate();
	if (Intake * HostMass * HostMass < 1e-6 * K)
	{
		return Data.Host.GetUnfedLifetime(HostMass);
	}
	const double Balance = FMath::Sqrt(K / Intake);
	const double Ratio = FMath::Min(HostMass / Balance, 1.0 - 1e-12);
	const double Artanh = 0.5 * FMath::Loge((1.0 + Ratio) / (1.0 - Ratio));
	return (Balance * Artanh - HostMass) / Intake;
}

bool FLRSimulation::AreInstrumentsDown() const
{
	return Data.Host.IsDefined() && (bVenting || Injector.Flow < 0.0);
}

bool FLRSimulation::CanVent() const
{
	return Data.Host.IsDefined() && !bVenting && !IsFrozen() && HostMass > 0.0;
}

FLRActionResult FLRSimulation::SetVenting(bool bVent)
{
	FLRActionResult Result;
	Result.Action = LRNames::Vent;
	if (bVent == bVenting)
	{
		Result.bSuccess = true;
		return Result;
	}
	if (!bVent)
	{
		bVenting = false;
		InjectorTarget = 0.0;
		RefreshAlarm(/*bAnnounce*/ false);
		Result.bSuccess = true;
		Result.Message = TEXT("Vent stopped: the dial is at OFF and the reversed injectors wind down. The instruments come back once they're still.");
		Complete(Result);
		return Result;
	}

	if (!CanVent())
	{
		FLRActionResult Failure = MakeFailure(LRNames::Vent, ReasonNoHost, FString::Printf(TEXT("Can't vent: %s"), *DescribeReason(ReasonNoHost)));
		Complete(Failure);
		return Failure;
	}

	bVenting = true;
	InjectorAuto = ELRInjectorAuto::Off;
	InjectorTarget = 0.0;
	RefreshAlarm(/*bAnnounce*/ false);
	Result.bSuccess = true;
	Result.Message = FString::Printf(TEXT("Venting: the injectors run in reverse. The graviton lens drives the horizon into stimulated emission, and the beamline draws the radiation off, so the host sheds mass as fast as the dial says (up to the rated limit). The radiation floods the chamber, so the instruments are down while it runs. Turn the dial up to start. Nothing stops it at the point of no return (%s): watch the host."),
		*FormatMass(GetTippingMass()));
	Complete(Result);
	return Result;
}

FString FLRSimulation::FormatMass(double Kg)
{
	if (Kg < 1000.0)
	{
		return FString::Printf(TEXT("%.0f kg"), FMath::Max(Kg, 0.0));
	}
	const double Tonnes = Kg / 1000.0;
	if (Tonnes >= 1e6)
	{
		return FString::Printf(TEXT("%.2f million t"), Tonnes / 1e6);
	}
	return FText::AsNumber(FMath::RoundToInt64(Tonnes)).ToString() + TEXT(" t");
}

FString FLRSimulation::FormatRate(double KgPerSecond)
{
	if (KgPerSecond < 1000.0)
	{
		return FString::Printf(TEXT("%.0f kg/s"), KgPerSecond);
	}
	return FString::Printf(TEXT("%.1f t/s"), KgPerSecond / 1000.0);
}

void FLRSimulation::AdvanceCosmos(double DeltaSeconds)
{
	if (IsFrozen())
	{
		return;
	}
	const FLREpochDef* Epoch = GetEpoch();
	if (!Epoch || Epoch->ClockSeconds <= 0.f || Epoch->StartTime <= 0.0)
	{
		return;
	}
	const double End = Epoch->EndTime > 0.0 ? Epoch->EndTime
		: (Data.Epochs.IsValidIndex(EpochIndex + 1) ? Data.Epochs[EpochIndex + 1].StartTime : 0.0);
	if (End <= Epoch->StartTime || CosmicTime >= End)
	{
		return;
	}
	// Log scale: the clock sweeps the same number of decades every second, then waits at the
	// end until the next epoch's requirements are met.
	const double Rate = (FMath::Loge(End) - FMath::Loge(Epoch->StartTime)) / Epoch->ClockSeconds;
	const double From = FMath::Max(CosmicTime, Epoch->StartTime);
	CosmicTime = FMath::Min(End, FMath::Exp(FMath::Loge(From) + Rate * DeltaSeconds));
}

void FLRSimulation::AdvanceStructures(double DeltaSeconds)
{
	const FLREpochDef* Epoch = GetEpoch();
	const FLRLootTableDef* YieldTable = (Epoch && !Epoch->YieldTable.IsNone()) ? Data.FindLootTable(Epoch->YieldTable) : nullptr;
	const double GrowthSeconds = Epoch ? static_cast<double>(Epoch->RippleGrowthSeconds) : 0.0;

	bool bMatterChanged = false;
	bool bWorldChanged = false;
	for (TPair<FIntVector, FLRPlacedEntity>& Pair : Placed)
	{
		FLRPlacedEntity& Entity = Pair.Value;
		const FLRItemDef* Def = Data.FindItem(Entity.Item);
		if (!Def || !Def->IsOverdensity())
		{
			continue;
		}

		// After recombination gravity deepens the ripples on its own; before it, radiation
		// pressure stops ordinary matter from clumping, so only Perturb can.
		if (GrowthSeconds > 0.0 && Entity.Amplitude < Def->MaxAmplitude)
		{
			Entity.GrowthProgress += DeltaSeconds;
			while (Entity.GrowthProgress >= GrowthSeconds && Entity.Amplitude < Def->MaxAmplitude)
			{
				Entity.GrowthProgress -= GrowthSeconds;
				++Entity.Amplitude;
				bWorldChanged = true;
			}
		}
		if (GrowthSeconds <= 0.0 || Entity.Amplitude >= Def->MaxAmplitude)
		{
			Entity.GrowthProgress = 0.0;
		}

		if (!YieldTable)
		{
			Entity.YieldProgress = 0.0;
			continue;
		}
		Entity.YieldProgress += DeltaSeconds;
		const double Interval = FMath::Max(0.1, static_cast<double>(Def->YieldSeconds));
		while (Entity.YieldProgress >= Interval)
		{
			Entity.YieldProgress -= Interval;
			// One roll per amplitude: a deeper ripple gathers more.
			TArray<FLRLootModifier> Modifiers;
			if (Entity.Amplitude > 1)
			{
				FLRLootModifier ExtraRolls;
				ExtraRolls.Kind = LRNames::ModifierExtraRolls;
				ExtraRolls.Value = static_cast<float>(Entity.Amplitude - 1);
				Modifiers.Add(ExtraRolls);
			}
			// The ripple gathers matter into its own cell.
			for (const FLRItemAmount& Amount : MergeAmounts(RollLootTable(ApplyModifiers(*YieldTable, Modifiers))))
			{
				AddMatter(Entity.Cell, Amount.Item, Amount.Count);
				AddStat(StatKey(TEXT("gained"), Amount.Item), Amount.Count);
				bMatterChanged = true;
			}
		}
	}

	if (bMatterChanged)
	{
		OnMatterChanged.Broadcast();
	}
	if (bWorldChanged)
	{
		OnWorldChanged.Broadcast();
	}
	if (bMatterChanged || bWorldChanged)
	{
		AnnounceUnlocks(RefreshUnlocks());
	}
}

double FLRSimulation::GetCosmicTemperature(double CosmicSeconds)
{
	const double Seconds = FMath::Max(CosmicSeconds, 1e-40);
	if (Seconds <= CosmicEqualitySeconds)
	{
		return CosmicRadiationTemperature / FMath::Sqrt(Seconds);
	}
	const double AtEquality = CosmicRadiationTemperature / FMath::Sqrt(CosmicEqualitySeconds);
	return AtEquality * FMath::Pow(Seconds / CosmicEqualitySeconds, -2.0 / 3.0);
}

double FLRSimulation::GetGasSpeedup() const
{
	const FLRGasDef& Gas = Data.Gas;
	if (!Gas.IsDefined())
	{
		return 0.0;
	}
	// Atoms move at sqrt(T / m): hotter gas spreads faster. The early universe is billions of
	// degrees, so the speed-up is capped.
	const double Ratio = GetCosmicTemperature(CosmicTime) / static_cast<double>(Gas.ReferenceTemperature);
	return FMath::Min(static_cast<double>(Gas.MaxSpeedup), FMath::Pow(Ratio, static_cast<double>(Gas.TemperatureExponent)));
}

void FLRSimulation::AdvanceGas(double DeltaSeconds)
{
	if (!Data.Gas.IsDefined())
	{
		GasProgress = 0.0;
		return;
	}
	const double Step = static_cast<double>(Data.Gas.StepSeconds);
	GasProgress += DeltaSeconds;
	bool bMoved = false;
	while (GasProgress >= Step)
	{
		GasProgress -= Step;
		bMoved |= StepGas(Step);
	}
	if (bMoved)
	{
		OnMatterChanged.Broadcast();
	}
}

bool FLRSimulation::IsGasIonized() const
{
	return Data.Gas.IonizationTemperature > 0.f && GetCosmicTemperature(CosmicTime) > static_cast<double>(Data.Gas.IonizationTemperature);
}

double FLRSimulation::GetGasSpeed(FName Item, bool bIonized) const
{
	const FLRItemDef* Def = Data.FindItem(Item);
	const double Mass = Def ? static_cast<double>(Def->AtomicMass) : 0.0;
	if (Mass <= 0.0)
	{
		return 0.0; // not a gas
	}
	// In the plasma everything moves with the light, whatever its mass. Neutral gas obeys
	// Graham's law: atoms move at 1 / sqrt(mass), relative to one of mass 1.
	return bIonized ? 1.0 : 1.0 / FMath::Sqrt(Mass);
}

FLRSimulation::FGasCell FLRSimulation::DescribeGas(const FLRCellMatter& CellMatter, bool bIonized, double Temperature) const
{
	FGasCell Out;
	for (const FLRItemAmount& Amount : CellMatter.Amounts)
	{
		const double Speed = GetGasSpeed(Amount.Item, bIonized);
		if (Speed <= 0.0)
		{
			continue;
		}
		const FLRItemDef* Def = Data.FindItem(Amount.Item);
		Out.Total += Amount.Count;
		Out.Particles += Amount.Count / static_cast<double>(Def->AtomicMass);
		// Particles times their speed: amount / mass^1.5 for neutral gas (speed^3 = mass^-1.5,
		// the amount / mass is folded in), just the amount in the plasma.
		Out.Escaping += Amount.Count * Speed * Speed * Speed;
	}
	// Jeans: gas collapses under its own gravity once its mass beats its pressure. With a
	// cell's volume fixed, the Jeans mass goes as temperature / mean particle mass. The plasma
	// never collapses (the light's pressure holds it up).
	const FLRGasDef& Gas = Data.Gas;
	if (!bIonized && Gas.JeansMass > 0.f && Out.Total > 0.0)
	{
		const double MeanMass = Out.Total / Out.Particles;
		Out.JeansMass = static_cast<double>(Gas.JeansMass) * (Temperature / static_cast<double>(Gas.ReferenceTemperature)) / MeanMass;
		Out.bBound = Out.Total > Out.JeansMass;
	}
	return Out;
}

bool FLRSimulation::IsGasBound(const FIntVector& Cell) const
{
	const FLRCellMatter* CellMatter = Matter.Find(Cell);
	return CellMatter && DescribeGas(*CellMatter, IsGasIonized(), GetCosmicTemperature(CosmicTime)).bBound;
}

double FLRSimulation::GetJeansMass(const FIntVector& Cell) const
{
	const FLRCellMatter* CellMatter = Matter.Find(Cell);
	return CellMatter ? DescribeGas(*CellMatter, IsGasIonized(), GetCosmicTemperature(CosmicTime)).JeansMass : 0.0;
}

void FLRSimulation::ForEachGasEdgeFlow(const FIntVector& A, const FGasCell& GasA, const FIntVector& B, const FGasCell& GasB,
	double SpreadShare, double InfallShare, bool bIonized, TFunctionRef<void(const FIntVector&, const FIntVector&, FName, double)> Visit) const
{
	// Share of each gas in From (scaled by its speed if bBySpeed) goes to To.
	auto Owe = [this, bIonized, &Visit](const FIntVector& From, const FIntVector& To, double Share, bool bBySpeed)
	{
		const FLRCellMatter* Source = Matter.Find(From);
		if (!Source)
		{
			return;
		}
		for (const FLRItemAmount& Amount : Source->Amounts)
		{
			const double Speed = GetGasSpeed(Amount.Item, bIonized);
			const double Flow = Share * Amount.Count * (bBySpeed ? Speed : (Speed > 0.0 ? 1.0 : 0.0));
			if (Flow > 0.0)
			{
				Visit(From, To, Amount.Item, Flow);
			}
		}
	};

	if (GasA.bBound || GasB.bBound)
	{
		// Gravity: a clump holds its gas and pulls in its neighbours' (all of it alike: gravity
		// doesn't care about mass). Of two clumps, the lighter falls into the heavier.
		if (GasA.bBound && GasB.bBound)
		{
			if (GasA.Total != GasB.Total)
			{
				const bool bAFalls = GasA.Total < GasB.Total;
				Owe(bAFalls ? A : B, bAFalls ? B : A, InfallShare, /*bBySpeed*/ false);
			}
		}
		else if (GasA.bBound && GasB.Total > 0.0)
		{
			Owe(B, A, InfallShare, /*bBySpeed*/ false);
		}
		else if (GasB.bBound && GasA.Total > 0.0)
		{
			Owe(A, B, InfallShare, /*bBySpeed*/ false);
		}
		return;
	}
	// Pressure: from the side where more escapes to the side where less does, in proportion to
	// the difference, each gas in proportion to how fast it escapes.
	if (GasA.Escaping == GasB.Escaping)
	{
		return;
	}
	const bool bAOut = GasA.Escaping > GasB.Escaping;
	const double Share = SpreadShare * FMath::Abs(GasA.Escaping - GasB.Escaping) / FMath::Max(GasA.Escaping, GasB.Escaping);
	Owe(bAOut ? A : B, bAOut ? B : A, Share, /*bBySpeed*/ true);
}

TMap<FName, double> FLRSimulation::GetMatterRates(const FIntVector& Cell) const
{
	TMap<FName, double> Rates;
	if (IsFrozen())
	{
		return Rates; // nothing moves in a frozen universe
	}

	// Gas crossing the cell's six edges right now.
	const FLRGasDef& Gas = Data.Gas;
	if (Gas.IsDefined())
	{
		const bool bIonized = IsGasIonized();
		const double Temperature = GetCosmicTemperature(CosmicTime);
		auto Describe = [this, bIonized, Temperature](const FIntVector& At)
		{
			const FLRCellMatter* CellMatter = Matter.Find(At);
			return CellMatter ? DescribeGas(*CellMatter, bIonized, Temperature) : FGasCell();
		};
		const FGasCell Here = Describe(Cell);
		const double SpreadShare = static_cast<double>(Gas.SpreadRate) * GetGasSpeedup();
		for (const FIntVector& Next : FLRHexGrid::Neighbors(Cell))
		{
			ForEachGasEdgeFlow(Cell, Here, Next, Describe(Next), SpreadShare, static_cast<double>(Gas.InfallRate), bIonized,
				[&Rates, &Cell](const FIntVector& From, const FIntVector& To, FName Item, double Flow)
				{
					Rates.FindOrAdd(Item) += (To == Cell) ? Flow : -Flow;
				});
		}
	}

	// A ripple's yield, on average: rolls (one per amplitude) x what a roll gives, every yieldSeconds.
	const FLRPlacedEntity* Entity = Placed.Find(Cell);
	const FLRItemDef* Def = Entity ? Data.FindItem(Entity->Item) : nullptr;
	const FLREpochDef* Epoch = GetEpoch();
	const FLRLootTableDef* YieldTable = (Epoch && !Epoch->YieldTable.IsNone()) ? Data.FindLootTable(Epoch->YieldTable) : nullptr;
	if (Def && Def->IsOverdensity() && YieldTable)
	{
		TArray<FLRLootModifier> Modifiers;
		if (Entity->Amplitude > 1)
		{
			FLRLootModifier ExtraRolls;
			ExtraRolls.Kind = LRNames::ModifierExtraRolls;
			ExtraRolls.Value = static_cast<float>(Entity->Amplitude - 1);
			Modifiers.Add(ExtraRolls);
		}
		const FLRLootTableDef Table = ApplyModifiers(*YieldTable, Modifiers);
		int32 TotalWeight = 0;
		for (const FLRLootEntry& Entry : Table.Entries)
		{
			TotalWeight += FMath::Max(0, Entry.Weight);
		}
		if (TotalWeight > 0)
		{
			const double Rolls = 0.5 * (FMath::Max(0, FMath::Min(Table.RollsMin, Table.RollsMax)) + FMath::Max(0, FMath::Max(Table.RollsMin, Table.RollsMax)));
			const double PerSecond = Rolls / FMath::Max(0.1, static_cast<double>(Def->YieldSeconds));
			for (const FLRLootEntry& Entry : Table.Entries)
			{
				const double Chance = static_cast<double>(FMath::Max(0, Entry.Weight)) / TotalWeight;
				Rates.FindOrAdd(Entry.Item) += PerSecond * Chance * 0.5 * (Entry.MinCount + Entry.MaxCount);
			}
		}
	}
	return Rates;
}

bool FLRSimulation::StepGas(double Seconds)
{
	const FLRGasDef& Gas = Data.Gas;
	const double SpreadShare = static_cast<double>(Gas.SpreadRate) * GetGasSpeedup() * Seconds;
	const double InfallShare = static_cast<double>(Gas.InfallRate) * Seconds;
	if (SpreadShare <= 0.0 || Matter.IsEmpty())
	{
		return false;
	}
	const bool bIonized = IsGasIonized();
	const double Temperature = GetCosmicTemperature(CosmicTime);

	TMap<FIntVector, FGasCell> Cells;
	for (const TPair<FIntVector, FLRCellMatter>& Pair : Matter)
	{
		const FGasCell Described = DescribeGas(Pair.Value, bIonized, Temperature);
		if (Described.Total > 0.0)
		{
			Cells.Add(Pair.Key, Described);
		}
	}

	for (const TPair<FIntVector, FGasCell>& Pair : Cells)
	{
		for (const FIntVector& Next : FLRHexGrid::Neighbors(Pair.Key))
		{
			const FGasCell* Found = Cells.Find(Next);
			if (Found && !IsGasEdgeFirst(Pair.Key, Next))
			{
				continue; // an edge between two cells with gas is done once, from its first cell
			}
			ForEachGasEdgeFlow(Pair.Key, Pair.Value, Next, Found ? *Found : FGasCell(), SpreadShare, InfallShare, bIonized,
				[this](const FIntVector& From, const FIntVector& To, FName Item, double Flow)
				{
					const bool bFromFirst = IsGasEdgeFirst(From, To);
					GasCarry.FindOrAdd(MakeTuple(bFromFirst ? From : To, bFromFirst ? To : From, Item)) += bFromFirst ? Flow : -Flow;
				});
		}
	}

	// Matter moves in whole units; the fractions wait for the next step.
	bool bMoved = false;
	for (auto It = GasCarry.CreateIterator(); It; ++It)
	{
		const FIntVector First = It->Key.Get<0>();
		const FIntVector Second = It->Key.Get<1>();
		const FName Item = It->Key.Get<2>();
		const int32 Whole = static_cast<int32>(It->Value); // towards zero
		if (Whole != 0)
		{
			const FIntVector& From = Whole > 0 ? First : Second;
			const FIntVector& To = Whole > 0 ? Second : First;
			FLRCellMatter* Source = Matter.Find(From);
			const int32 Moved = Source ? FMath::Min(FMath::Abs(Whole), Source->Get(Item)) : 0;
			if (Moved > 0)
			{
				Source->Add(Item, -Moved);
				if (Source->IsEmpty())
				{
					Matter.Remove(From);
				}
				AddMatter(To, Item, Moved);
				bMoved = true;
			}
			// Whatever couldn't be paid (the matter already left by another edge) is dropped:
			// only the fraction carries over.
			It->Value -= Whole;
		}
		if (GetMatter(First, Item) == 0 && GetMatter(Second, Item) == 0)
		{
			It.RemoveCurrent(); // neither side has this gas any more
		}
	}
	return bMoved;
}

void FLRSimulation::AnnounceEvents(FName Event, const TArray<FString>& Messages)
{
	for (const FString& Message : Messages)
	{
		FLRActionResult Result;
		Result.Action = Event;
		Result.bSuccess = true;
		Result.Message = Message;
		OnActionCompleted.Broadcast(Result);
	}
}

float FLRSimulation::GetPlasmaOpacity() const
{
	const FLREpochDef* Epoch = GetEpoch();
	if (!Epoch)
	{
		return 0.f;
	}
	if (EpochIndex == 0)
	{
		return Epoch->Plasma;
	}
	const float Previous = Data.Epochs[EpochIndex - 1].Plasma;
	const float Alpha = static_cast<float>(FMath::Clamp(GetEpochAge() / PlasmaFadeSeconds, 0.0, 1.0));
	return FMath::Lerp(Previous, Epoch->Plasma, Alpha);
}

FString FLRSimulation::FormatCosmicTime(double Seconds)
{
	constexpr double Minute = 60.0;
	constexpr double Hour = 3600.0;
	constexpr double Day = 86400.0;
	constexpr double Year = 3.15576e7; // Julian year

	if (Seconds <= 0.0)
	{
		return TEXT("0 s");
	}
	if (Seconds < 0.01)
	{
		// The nudge keeps exact powers of ten (1e-36) from rounding down a decade.
		const int32 Exponent = FMath::FloorToInt32(FMath::Loge(Seconds) / FMath::Loge(10.0) + 1e-9);
		return FString::Printf(TEXT("10^%d s"), Exponent);
	}
	if (Seconds < 1.0)    { return FString::Printf(TEXT("%.2f s"), Seconds); }
	if (Seconds < Minute) { return FString::Printf(TEXT("%.0f s"), Seconds); }
	if (Seconds < Hour)   { return FString::Printf(TEXT("%.0f min"), Seconds / Minute); }
	if (Seconds < Day)    { return FString::Printf(TEXT("%.0f hours"), Seconds / Hour); }
	if (Seconds < Year)   { return FString::Printf(TEXT("%.0f days"), Seconds / Day); }

	const double Years = Seconds / Year;
	if (Years < 1e3)      { return FString::Printf(TEXT("%.0f years"), Years); }
	if (Years < 1e6)      { return FString::Printf(TEXT("%.0f thousand years"), Years / 1e3); }
	if (Years < 1e9)      { return FString::Printf(TEXT("%.0f million years"), Years / 1e6); }
	return FString::Printf(TEXT("%.1f billion years"), Years / 1e9);
}

void FLRSimulation::AdvanceIrradiation(double DeltaSeconds)
{
	TArray<FString> Messages;
	for (TPair<FIntVector, FLRPlacedEntity>& Pair : Placed)
	{
		FLRPlacedEntity& Entity = Pair.Value;
		const FLRItemDef* IrradiatorDef = Data.FindItem(Entity.Item);
		if (!IrradiatorDef || !IrradiatorDef->IsIrradiator() || Entity.Chamber.IsEmpty() || Entity.Source.IsEmpty())
		{
			continue;
		}
		FLRLootBoxInstance* Box = LootBoxes.Find(Entity.Chamber.InstanceId);
		const FLRItemDef* SourceDef = Data.FindItem(Entity.Source.Item);
		const FLRRadiationDef* Radiation = SourceDef ? Data.FindRadiation(SourceDef->Radiation) : nullptr;
		if (!Box || !Radiation || Radiation->Effect.IsNone() || IsExposureComplete(*Box, *IrradiatorDef))
		{
			Entity.ExposureProgress = 0.0;
			continue;
		}

		Entity.ExposureProgress += DeltaSeconds;
		const double Interval = FMath::Max(0.1, static_cast<double>(IrradiatorDef->ExposureSeconds));
		while (Entity.ExposureProgress >= Interval && !IsExposureComplete(*Box, *IrradiatorDef))
		{
			Entity.ExposureProgress -= Interval;
			Messages.Add(FString::Printf(TEXT("%s at %s: %s"),
				*Data.GetDisplayName(Entity.Chamber.Item), *DescribeCell(Entity.Cell), *ApplyExposure(*Box, *Radiation, *IrradiatorDef)));
		}
		if (IsExposureComplete(*Box, *IrradiatorDef))
		{
			Entity.ExposureProgress = 0.0;
		}
	}

	if (Messages.IsEmpty())
	{
		return;
	}
	OnWorldChanged.Broadcast();
	for (const FString& Message : Messages)
	{
		FLRActionResult Result;
		Result.Action = IrradiateEvent;
		Result.bSuccess = true;
		Result.Message = Message;
		OnActionCompleted.Broadcast(Result);
	}
	AnnounceUnlocks(RefreshUnlocks());
}

bool FLRSimulation::IsExposureComplete(const FLRLootBoxInstance& Box, const FLRItemDef& IrradiatorDef)
{
	return Box.bRevealed || Box.Modifiers.Num() >= IrradiatorDef.MaxExposureStacks;
}

FString FLRSimulation::ApplyExposure(FLRLootBoxInstance& Box, const FLRRadiationDef& Radiation, const FLRItemDef& IrradiatorDef)
{
	if (Radiation.Effect.Kind == LRNames::ModifierReveal)
	{
		// X-ray inspection: roll the (modified) table now and lock the result in.
		const FLRLootTableDef* Table = Data.FindLootTable(Box.LootTable);
		if (!Table)
		{
			Table = Data.FindLootTable(LRNames::DefaultLootTable);
		}
		Box.RevealedContents = Table ? MergeAmounts(RollLootTable(ApplyModifiers(*Table, Box.Modifiers))) : TArray<FLRItemAmount>();
		Box.bRevealed = true;
		AddStat(StatKey(TEXT("exposed"), Radiation.Id));
		return FString::Printf(TEXT("%s observed it and collapsed its contents: %s (fixed)"), *Radiation.Name,
			Box.RevealedContents.IsEmpty() ? TEXT("nothing!") : *DescribeAmounts(Box.RevealedContents));
	}

	AddStat(StatKey(TEXT("exposed"), Radiation.Id));
	FLRLootModifier Modifier = Radiation.Effect;
	Modifier.Source = Radiation.Id;
	Box.Modifiers.Add(Modifier);
	return FString::Printf(TEXT("absorbed %s, %s (%d/%d)"), *Radiation.Name, *DescribeModifier(Modifier, Data),
		Box.Modifiers.Num(), IrradiatorDef.MaxExposureStacks);
}

// ---------------------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------------------

const FLRLootBoxInstance* FLRSimulation::FindCacheAt(const FIntVector& Cell) const
{
	const FLRPlacedEntity* Entity = Placed.Find(Cell);
	const FLRItemDef* Def = Entity ? Data.FindItem(Entity->Item) : nullptr;
	if (Def && Def->IsLootBox())
	{
		return LootBoxes.Find(Entity->InstanceId);
	}
	if (Def && Def->IsIrradiator() && !Entity->Chamber.IsEmpty())
	{
		return LootBoxes.Find(Entity->Chamber.InstanceId);
	}
	return nullptr;
}

int32 FLRSimulation::GetMatter(const FIntVector& Cell, FName Item) const
{
	const FLRCellMatter* CellMatter = Matter.Find(Cell);
	return CellMatter ? CellMatter->Get(Item) : 0;
}

int32 FLRSimulation::GetTotalMatter(FName Item) const
{
	int32 Total = 0;
	for (const TPair<FIntVector, FLRCellMatter>& Pair : Matter)
	{
		Total += Pair.Value.Get(Item);
	}
	return Total;
}

int32 FLRSimulation::GetMatterInReach(const FIntVector& Cell, FName Item) const
{
	// Few cells hold matter, so walk those rather than every cell in reach.
	const int32 Radius = GetReachRadius();
	int32 Total = 0;
	for (const TPair<FIntVector, FLRCellMatter>& Pair : Matter)
	{
		if (Pair.Key.Z == Cell.Z && FLRHexGrid::Distance(Pair.Key, Cell) <= Radius)
		{
			Total += Pair.Value.Get(Item);
		}
	}
	return Total;
}

bool FLRSimulation::CanAffordAt(const FIntVector& Cell, const TArray<FLRItemAmount>& Cost) const
{
	for (const FLRItemAmount& Amount : MergeAmounts(Cost))
	{
		if (GetMatterInReach(Cell, Amount.Item) < Amount.Count)
		{
			return false;
		}
	}
	return true;
}

FName FLRSimulation::ValidateBuild(FName RecipeId, const FIntVector& Cell, bool bCheckCost) const
{
	const FLRRecipeDef* Recipe = Data.FindRecipe(RecipeId);
	if (!Recipe) { return ReasonUnknownRecipe; }
	if (!IsRecipeUnlocked(RecipeId)) { return ReasonRecipeLocked; }
	const FLRItemDef* Output = Data.FindItem(Recipe->Output);
	if (!Output || Output->IsStructure()) { return ReasonUnknownRecipe; }

	// Where the output goes: sources into the irradiator in the cell, caches into an empty cell
	// or an irradiator's empty chamber, machines into an empty cell, materials anywhere.
	const FLRPlacedEntity* Entity = Placed.Find(Cell);
	const FLRItemDef* EntityDef = Entity ? Data.FindItem(Entity->Item) : nullptr;
	const bool bIrradiator = EntityDef && EntityDef->IsIrradiator();
	if (Output->IsSource())
	{
		if (!bIrradiator) { return ReasonNeedsIrradiator; }
		if (!Entity->Source.IsEmpty()) { return ReasonSourceFull; }
		const FLRRadiationDef* Radiation = Data.FindRadiation(Output->Radiation);
		if (!Radiation || Radiation->Tier > EntityDef->MaxRadiationTier) { return ReasonTooStrong; }
	}
	else if (Output->IsLootBox())
	{
		if (Entity && !bIrradiator) { return ReasonOccupied; }
		if (bIrradiator && !Entity->Chamber.IsEmpty()) { return ReasonChamberFull; }
	}
	else if (Output->Category != LRNames::CategoryMaterial && Entity)
	{
		return ReasonOccupied;
	}
	return (!bCheckCost || CanAffordAt(Cell, Recipe->Cost)) ? NAME_None : ReasonInsufficientMaterials;
}

FName FLRSimulation::CheckBuildsAt(const FIntVector& Cell) const
{
	if (IsCellBusy(Cell))
	{
		return ReasonCellBusy;
	}
	for (const FLRRecipeDef& Recipe : Data.Recipes)
	{
		if (ValidateBuild(Recipe.Id, Cell, /*bCheckCost*/ false).IsNone())
		{
			return NAME_None;
		}
	}
	return ReasonNoBuilds;
}

// ---------------------------------------------------------------------------------------
// Tech tree / stats
// ---------------------------------------------------------------------------------------

FName FLRSimulation::StatKey(const TCHAR* Prefix, FName Id)
{
	return FName(*FString::Printf(TEXT("%s:%s"), Prefix, *Id.ToString()));
}

int32 FLRSimulation::GetStat(FName Key) const
{
	const int32* Value = Stats.Find(Key);
	return Value ? *Value : 0;
}

void FLRSimulation::AddStat(FName Key, int32 Delta)
{
	Stats.FindOrAdd(Key) += Delta;
}

bool FLRSimulation::IsRecipeUnlocked(FName RecipeId) const
{
	const FLRRecipeDef* Recipe = Data.FindRecipe(RecipeId);
	return Recipe && (Recipe->RevealRequirements.IsEmpty() || Unlocked.Contains(FLRGameData::RecipeUnlockKey(RecipeId)));
}

bool FLRSimulation::IsActionUnlocked(FName ActionName) const
{
	const FLRActionDef* Action = Data.FindAction(ActionName);
	return Action && (Action->RevealRequirements.IsEmpty() || Unlocked.Contains(FLRGameData::ActionUnlockKey(ActionName)));
}

bool FLRSimulation::IsActionRetired(FName ActionName) const
{
	return Unlocked.Contains(FLRGameData::ActionRetiredKey(ActionName));
}

TArray<FString> FLRSimulation::RefreshUnlocks()
{
	// Unlocks latch: once met, they stay. Loop so chains ("unlocked" requirements) resolve
	// in one pass no matter how the data is ordered.
	TArray<FString> Messages;
	bool bChanged = true;
	while (bChanged)
	{
		bChanged = false;
		for (const FLRRecipeDef& Recipe : Data.Recipes)
		{
			const FName Key = FLRGameData::RecipeUnlockKey(Recipe.Id);
			if (!Recipe.RevealRequirements.IsEmpty() && !Unlocked.Contains(Key) && CheckRequirements(Recipe.RevealRequirements))
			{
				Unlocked.Add(Key);
				Messages.Add(FString::Printf(TEXT("New recipe unlocked: %s"), *Recipe.Label));
				bChanged = true;
			}
		}
		for (const FLRActionDef& Action : Data.Actions)
		{
			const FName Key = FLRGameData::ActionUnlockKey(Action.Name);
			if (!Action.RevealRequirements.IsEmpty() && !Unlocked.Contains(Key) && CheckRequirements(Action.RevealRequirements))
			{
				Unlocked.Add(Key);
				Messages.Add(FString::Printf(TEXT("New action unlocked: %s"), *Action.Label));
				bChanged = true;
			}
			const FName RetiredKey = FLRGameData::ActionRetiredKey(Action.Name);
			if (!Action.RetireRequirements.IsEmpty() && !Unlocked.Contains(RetiredKey) && CheckRequirements(Action.RetireRequirements))
			{
				Unlocked.Add(RetiredKey);
				Messages.Add(FString::Printf(TEXT("Action retired: %s is no longer needed"), *Action.Label));
				bChanged = true;
			}
		}
		// Epochs advance one at a time, in order.
		if (Data.Epochs.IsValidIndex(EpochIndex + 1) && CheckRequirements(Data.Epochs[EpochIndex + 1].AdvanceRequirements))
		{
			EnterEpoch(EpochIndex + 1);
			const FLREpochDef& Epoch = Data.Epochs[EpochIndex];
			Messages.Add(FString::Printf(TEXT("New epoch: %s, %s after the Big Bang. %s"),
				*Epoch.Name, *FormatCosmicTime(CosmicTime), *Epoch.Description));
			bChanged = true;
		}
	}
	return Messages;
}

void FLRSimulation::AnnounceUnlocks(const TArray<FString>& Messages)
{
	for (const FString& Message : Messages)
	{
		FLRActionResult Result;
		Result.Action = UnlockEvent;
		Result.bSuccess = true;
		Result.Message = Message;
		OnActionCompleted.Broadcast(Result);
	}
}

bool FLRSimulation::CheckRequirement(const FLRRequirement& Requirement) const
{
	int32 Actual = 0;
	if (Requirement.Check == LRNames::CheckStat)
	{
		Actual = GetStat(Requirement.Id);
	}
	else if (Requirement.Check == LRNames::CheckUnlocked)
	{
		Actual = Unlocked.Contains(Requirement.Id) ? 1 : 0;
	}
	else if (Requirement.Check == LRNames::CheckEpoch)
	{
		const int32 Index = Data.FindEpochIndex(Requirement.Id);
		Actual = (Index != INDEX_NONE && EpochIndex >= Index) ? 1 : 0;
	}
	else if (Requirement.Check == LRNames::CheckHost)
	{
		Actual = FMath::FloorToInt32(HostMass / 1000.0); // tonnes
	}
	else if (Requirement.Check == LRNames::CheckPlaced)
	{
		for (const TPair<FIntVector, FLRPlacedEntity>& Pair : Placed)
		{
			const FLRItemDef* Def = Data.FindItem(Pair.Value.Item);
			const bool bItemMatches = Requirement.Item.IsNone() || Pair.Value.Item == Requirement.Item;
			const bool bCategoryMatches = Requirement.Category.IsNone() || (Def && Def->Category == Requirement.Category);
			if (bItemMatches && bCategoryMatches)
			{
				++Actual;
			}
		}
	}
	else if (Requirement.Check == LRNames::CheckMatter)
	{
		if (!Requirement.Item.IsNone())
		{
			Actual = GetTotalMatter(Requirement.Item);
		}
		else
		{
			for (const TPair<FName, FLRItemDef>& Pair : Data.Items)
			{
				Actual += Pair.Value.Category == Requirement.Category ? GetTotalMatter(Pair.Key) : 0;
			}
		}
	}
	return FLRRequirement::EvaluateCondition(Actual, Requirement.Condition, Requirement.Value);
}

bool FLRSimulation::CheckRequirements(const TArray<FLRRequirement>& Requirements) const
{
	for (const FLRRequirement& Requirement : Requirements)
	{
		if (!CheckRequirement(Requirement))
		{
			return false;
		}
	}
	return true;
}

FLRActionStatus FLRSimulation::GetActionStatus(FName ActionName) const
{
	FLRActionStatus Status;
	Status.Name = ActionName;

	const FLRActionDef* Def = Data.FindAction(ActionName);
	if (!Def)
	{
		return Status;
	}

	Status.Label = Def->Label;
	Status.Tooltip = Def->Tooltip;
	Status.CastTime = Def->CastTime;
	Status.bRevealed = IsActionUnlocked(ActionName) && !IsActionRetired(ActionName); // latched tech-tree unlock

	// Whether this recipe or that cell works is checked per request (ValidateBuild etc.).
	Status.bRequirementsMet = CheckRequirements(Def->Requirements);

	// Cooldown: the global one (shared by every action), or the action's own if it has one and
	// it's longer.
	const FLRActionState* State = ActionStates.Find(ActionName);
	const double OwnRemaining = State ? FMath::Max(0.0, State->CooldownUntil - Now) : 0.0;
	const double GlobalRemaining = FMath::Max(0.0, GlobalCooldownUntil - Now);
	Status.CooldownRemaining = static_cast<float>(FMath::Max(OwnRemaining, GlobalRemaining));
	Status.Cooldown = OwnRemaining > GlobalRemaining ? Def->Cooldown : Data.GlobalCooldown;
	Status.bOnCooldown = Status.CooldownRemaining > 0.f;

	Status.bInstrumentsDown = AreInstrumentsDown();
	Status.bEnabled = Status.bRevealed && Status.bRequirementsMet && !Status.bOnCooldown && !Status.bInstrumentsDown;
	return Status;
}

// ---------------------------------------------------------------------------------------
// Commands
// ---------------------------------------------------------------------------------------

FLRActionResult FLRSimulation::MakeFailure(FName Action, FName Reason, const FString& Message)
{
	FLRActionResult Result;
	Result.Action = Action;
	Result.bSuccess = false;
	Result.Reason = Reason;
	Result.Message = Message;
	return Result;
}

void FLRSimulation::Complete(const FLRActionResult& Result)
{
	TArray<FString> NewUnlocks;
	if (Result.bSuccess && !Result.bStarted)
	{
		AddStat(StatKey(TEXT("done"), Result.Action));
		for (const FLRItemAmount& Amount : Result.Gained)
		{
			AddStat(StatKey(TEXT("gained"), Amount.Item), Amount.Count);
		}
		NewUnlocks = RefreshUnlocks();
	}
	if (Result.bSuccess)
	{
		OnMatterChanged.Broadcast();
		OnWorldChanged.Broadcast();
	}
	OnActionCompleted.Broadcast(Result);
	AnnounceUnlocks(NewUnlocks);
}

FLRActionResult FLRSimulation::RequestAction(const FLRActionRequest& Request)
{
	const FLRActionDef* Def = Data.FindAction(Request.Action);
	if (!Def)
	{
		FLRActionResult Result = MakeFailure(Request.Action, ReasonUnknownAction,
			FString::Printf(TEXT("Unknown action '%s'"), *Request.Action.ToString()));
		Complete(Result);
		return Result;
	}

	const FName Reason = CheckRequest(Request);

	if (!Reason.IsNone())
	{
		FLRActionResult Result = MakeFailure(Request.Action, Reason,
			FString::Printf(TEXT("Can't %s: %s"), *Def->Label.ToLower(), *DescribeReason(Reason)));
		Complete(Result);
		return Result;
	}

	// A job pays when it starts (and gets it back if it fails). An instant action pays as it runs.
	const bool bJob = Def->CastTime > 0.f && Request.bHasCell;
	FLRCostTransaction Paid;
	if (bJob && !PayForJob(Request, Paid))
	{
		FLRActionResult Result = MakeFailure(Request.Action, ReasonInsufficientMaterials,
			FString::Printf(TEXT("Can't %s: %s"), *Def->Label.ToLower(), *DescribeReason(ReasonInsufficientMaterials)));
		Complete(Result);
		return Result;
	}

	GlobalCooldownUntil = Now + Data.GlobalCooldown;
	if (Def->Cooldown > 0.f)
	{
		FLRActionState& State = ActionStates.FindOrAdd(Request.Action);
		State.Name = Request.Action;
		State.CooldownUntil = Now + Def->Cooldown;
	}

	if (!bJob)
	{
		// Rails: PerformPlayerActionJob.perform_now
		FLRActionResult Result = Execute(Request);
		Complete(Result);
		return Result;
	}

	// A job in the cell: it's carried out when its time is up (Advance), and the cell is busy
	// until then. Rails: PerformPlayerActionJob.set(wait: cast_time).perform_later
	FLRCellJob& Job = Jobs.Add(Request.Cell);
	Job.Request = Request;
	Job.StartedAt = Now;
	Job.EndsAt = Now + Def->CastTime;
	Job.Paid = Paid;
	if (!Paid.Matter.IsEmpty())
	{
		OnMatterChanged.Broadcast(); // the cost left its cells now
	}

	FLRActionResult Result;
	Result.Action = Request.Action;
	Result.bSuccess = true;
	Result.bStarted = true;
	Result.Message = FString::Printf(TEXT("%s in %s..."), *Def->Label, *DescribeCell(Request.Cell));
	return Result;
}

FName FLRSimulation::CheckRequest(const FLRActionRequest& Request) const
{
	if (!Data.FindAction(Request.Action))
	{
		return ReasonUnknownAction;
	}
	const FLRActionStatus Status = GetActionStatus(Request.Action);
	if (IsFrozen())                                 { return ReasonGameOver; }
	if (!Status.bRevealed)                          { return ReasonNotRevealed; }
	if (Status.bInstrumentsDown)                    { return ReasonInstrumentsDown; }
	if (Status.bOnCooldown)                         { return ReasonOnCooldown; }
	if (Request.bHasCell && IsCellBusy(Request.Cell)) { return ReasonCellBusy; }
	if (!Status.bRequirementsMet)                   { return ReasonRequirements; }
	return ValidateRequest(Request);
}

FName FLRSimulation::ValidateRequest(const FLRActionRequest& Request) const
{
	const FLRActionDef* ActionDef = Data.FindAction(Request.Action);
	if (ActionDef && !ActionDef->Places.IsNone())
	{
		return ValidateSeed(Request, *ActionDef);
	}
	// Everything else acts on a cell.
	if (!Request.bHasCell) { return ReasonNoCell; }
	if (Request.Action == LRNames::Craft)
	{
		return ValidateBuild(Request.Choice, Request.Cell);
	}
	if (Request.Action == LRNames::Use)
	{
		return FindCacheAt(Request.Cell) ? NAME_None : ReasonNoLootBox;
	}
	if (Request.Action == LRNames::Dismantle)
	{
		const FLRPlacedEntity* Entity = Placed.Find(Request.Cell);
		if (!Entity) { return ReasonNothingPlaced; }
		const FLRItemDef* EntityDef = Data.FindItem(Entity->Item);
		return (EntityDef && EntityDef->IsStructure()) ? ReasonCantDismantle : NAME_None;
	}
	return NAME_None;
}

FLRActionResult FLRSimulation::Execute(const FLRActionRequest& Request, bool bPrepaid)
{
	// Rails: PerformPlayerActionJob -> user.send(action_name, action_data)
	const FLRActionDef* Def = Data.FindAction(Request.Action);
	if (!Def)
	{
		return MakeFailure(Request.Action, ReasonUnknownAction, TEXT("Unknown action"));
	}
	if (Request.Action == LRNames::Craft)     { return ExecuteCraft(Request, bPrepaid); }
	if (Request.Action == LRNames::Use)       { return ExecuteUse(Request); }
	if (Request.Action == LRNames::Dismantle) { return ExecuteDismantle(Request); }
	if (!Def->Places.IsNone())                { return ExecuteSeed(Request, *Def, bPrepaid); }
	if (!Def->LootTable.IsNone())             { return ExecuteLootAction(Request, *Def); }

	return MakeFailure(Request.Action, ReasonUnknownAction,
		FString::Printf(TEXT("No behaviour implemented for '%s'"), *Request.Action.ToString()));
}

namespace
{
	FString DescribeLost(int32 Lost)
	{
		return Lost > 0 ? FString::Printf(TEXT(" (%d cache(s) lost: no empty cell within reach)"), Lost) : FString();
	}
}

FLRActionResult FLRSimulation::ExecuteLootAction(const FLRActionRequest& Request, const FLRActionDef& Def)
{
	// Rails: InventoryItem.scavenge_item - now a roll on a loot table, landing in the selected cell.
	if (!Request.bHasCell)
	{
		return MakeFailure(Def.Name, ReasonNoCell, FString::Printf(TEXT("Can't %s: %s"), *Def.Label.ToLower(), *DescribeReason(ReasonNoCell)));
	}
	const FLRLootTableDef* Table = Data.FindLootTable(Def.LootTable);
	if (!Table)
	{
		return MakeFailure(Def.Name, ReasonNoLootTable, FString::Printf(TEXT("%s has no loot table"), *Def.Label));
	}

	const TArray<FLRItemAmount> Rolled = MergeAmounts(RollLootTable(*Table));
	const int32 Lost = Spill(Request.Cell, Rolled);

	FLRActionResult Result;
	Result.Action = Def.Name;
	Result.bSuccess = true;
	Result.Gained = Rolled;
	Result.Message = FString::Printf(TEXT("%s at %s: %s%s"), *Def.Label, *DescribeCell(Request.Cell), *DescribeAmounts(Rolled), *DescribeLost(Lost));
	return Result;
}

FLRActionResult FLRSimulation::ExecuteCraft(const FLRActionRequest& Request, bool bPrepaid)
{
	// Rails: Entity.craft_item. Build at the cell, paying from the matter within reach of it.
	const FLRRecipeDef* Recipe = Data.FindRecipe(Request.Choice);
	const FString Label = Recipe ? Recipe->Label : Request.Choice.ToString();
	const FName Invalid = Request.bHasCell ? ValidateBuild(Request.Choice, Request.Cell, /*bCheckCost*/ !bPrepaid) : ReasonNoCell;
	if (!Invalid.IsNone())
	{
		return MakeFailure(Request.Action, Invalid, FString::Printf(TEXT("Can't build %s: %s"), *Label, *DescribeReason(Invalid)));
	}
	const FLRItemDef* Output = Data.FindItem(Recipe->Output);
	check(Output); // ValidateBuild checked it
	const int32 OutputCount = FMath::Max(1, Recipe->OutputCount);

	FTransaction Txn(*this);
	if (!bPrepaid && !RemoveMatterInReach(Request.Cell, Recipe->Cost))
	{
		return MakeFailure(Request.Action, ReasonInsufficientMaterials,
			FString::Printf(TEXT("Can't build %s: %s"), *Label, *DescribeReason(ReasonInsufficientMaterials)));
	}
	if (Output->Category == LRNames::CategoryMaterial)
	{
		AddMatter(Request.Cell, Output->Id, OutputCount);
	}
	else if (Output->IsSource())
	{
		FLRPlacedEntity& Irradiator = Placed.FindChecked(Request.Cell);
		Irradiator.Source.Item = Output->Id;
		Irradiator.Source.Count = 1;
		Irradiator.Source.InstanceId = AllocateInstanceId();
	}
	else if (Output->IsLootBox())
	{
		if (!PlaceNewCache(Request.Cell, Output->Id))
		{
			return MakeFailure(Request.Action, ReasonOccupied, FString::Printf(TEXT("Can't build %s: %s"), *Label, *DescribeReason(ReasonOccupied)));
		}
	}
	else
	{
		FLRPlacedEntity Entity;
		Entity.InstanceId = AllocateInstanceId();
		Entity.Item = Output->Id;
		Entity.Cell = Request.Cell;
		Entity.PlacedAt = Now;
		Placed.Add(Entity.Cell, Entity);
	}
	Txn.Commit();

	FLRActionResult Result;
	Result.Action = Request.Action;
	Result.bSuccess = true;
	Result.Spent = Recipe->Cost;
	Result.Gained.Emplace(Output->Id, OutputCount);
	Result.Message = FString::Printf(TEXT("Built %s at %s"), *DescribeAmounts(Result.Gained), *DescribeCell(Request.Cell));
	AddStat(StatKey(TEXT("crafted"), Recipe->Id));
	return Result;
}

FLRActionResult FLRSimulation::ExecuteUse(const FLRActionRequest& Request)
{
	// Rails: User#use + LootBox#open!. The cache collapses where it is, and its contents land
	// in that cell.
	FLRPlacedEntity* Entity = Request.bHasCell ? Placed.Find(Request.Cell) : nullptr;
	const FLRItemDef* EntityDef = Entity ? Data.FindItem(Entity->Item) : nullptr;
	const bool bInIrradiator = EntityDef && EntityDef->IsIrradiator() && !Entity->Chamber.IsEmpty();
	if (!EntityDef || (!EntityDef->IsLootBox() && !bInIrradiator))
	{
		return MakeFailure(Request.Action, ReasonNoLootBox, FString::Printf(TEXT("Can't open: %s"), *DescribeReason(ReasonNoLootBox)));
	}
	const FIntVector Cell = Request.Cell;
	const int32 BoxId = bInIrradiator ? Entity->Chamber.InstanceId : Entity->InstanceId;
	const FName BoxItem = bInIrradiator ? Entity->Chamber.Item : Entity->Item;
	const FLRItemDef* BoxDef = Data.FindItem(BoxItem);

	FLRLootBoxInstance Instance;
	if (const FLRLootBoxInstance* Found = LootBoxes.Find(BoxId))
	{
		Instance = *Found;
	}
	else
	{
		// A cache whose instance went missing opens on its item's own table, unmodified.
		Instance.InstanceId = BoxId;
		Instance.LootTable = BoxDef ? BoxDef->LootTable : LRNames::DefaultLootTable;
	}
	const FLRLootTableDef* Table = Data.FindLootTable(Instance.LootTable);
	if (!Table)
	{
		Table = Data.FindLootTable(LRNames::DefaultLootTable);
	}
	if (!Table)
	{
		return MakeFailure(Request.Action, ReasonNoLootTable, TEXT("Can't open: loot table missing"));
	}

	// X-rayed boxes open to exactly what was revealed.
	const TArray<FLRItemAmount> Rolled = Instance.bRevealed
		? Instance.RevealedContents
		: MergeAmounts(RollLootTable(ApplyModifiers(*Table, Instance.Modifiers)));

	LootBoxes.Remove(BoxId);
	if (bInIrradiator)
	{
		Entity->Chamber.Clear();
		Entity->ExposureProgress = 0.0;
	}
	else
	{
		Placed.Remove(Cell); // Entity is gone from here on
	}
	const int32 Lost = Spill(Cell, Rolled);

	FLRActionResult Result;
	Result.Action = Request.Action;
	Result.bSuccess = true;
	Result.Spent.Emplace(BoxItem, 1);
	Result.Gained = Rolled;
	AddStat(StatKey(TEXT("opened"), BoxItem));
	Result.Message = FString::Printf(TEXT("Opened %s at %s: %s%s"), *Data.GetDisplayName(BoxItem), *DescribeCell(Cell),
		Rolled.IsEmpty() ? TEXT("nothing!") : *DescribeAmounts(Rolled), *DescribeLost(Lost));
	return Result;
}

FLRActionResult FLRSimulation::ExecuteDismantle(const FLRActionRequest& Request)
{
	// Take apart what's in the cell and return what it cost to the cell's matter. An irradiator
	// comes apart one layer at a time: its source, then its cache, then the irradiator itself.
	const FName Invalid = ValidateRequest(Request);
	if (!Invalid.IsNone())
	{
		return MakeFailure(Request.Action, Invalid, FString::Printf(TEXT("Can't dismantle: %s"), *DescribeReason(Invalid)));
	}
	const FIntVector Cell = Request.Cell;
	FLRPlacedEntity& Entity = Placed.FindChecked(Cell);
	const FLRItemDef* Def = Data.FindItem(Entity.Item);
	const bool bIrradiator = Def && Def->IsIrradiator();

	FName Removed;
	if (bIrradiator && !Entity.Source.IsEmpty())
	{
		Removed = Entity.Source.Item;
		Entity.Source.Clear();
		Entity.ExposureProgress = 0.0;
	}
	else if (bIrradiator && !Entity.Chamber.IsEmpty())
	{
		Removed = Entity.Chamber.Item;
		LootBoxes.Remove(Entity.Chamber.InstanceId);
		Entity.Chamber.Clear();
		Entity.ExposureProgress = 0.0;
	}
	else
	{
		Removed = Entity.Item;
		LootBoxes.Remove(Entity.InstanceId); // no-op unless it's a cache
		Placed.Remove(Cell); // Entity is gone from here on
	}
	const TArray<FLRItemAmount> Refunded = Refund(Cell, Removed);

	FLRActionResult Result;
	Result.Action = Request.Action;
	Result.bSuccess = true;
	Result.Spent.Emplace(Removed, 1);
	Result.Message = FString::Printf(TEXT("Dismantled %s at %s: %s"), *Data.GetDisplayName(Removed), *DescribeCell(Cell),
		Refunded.IsEmpty() ? TEXT("nothing to recover") : *FString::Printf(TEXT("%s back in the cell"), *DescribeAmounts(Refunded)));
	return Result;
}

FName FLRSimulation::ValidateSeed(const FLRActionRequest& Request, const FLRActionDef& Def, bool bCheckCost) const
{
	const FLRItemDef* StructureDef = Data.FindItem(Def.Places);
	if (!StructureDef || !StructureDef->IsOverdensity()) { return ReasonUnknownAction; }
	if (!Request.bHasCell) { return ReasonNoCell; }
	const FLRPlacedEntity* Existing = Placed.Find(Request.Cell);
	if (Existing)
	{
		if (Existing->Item != Def.Places) { return ReasonOccupied; }
		if (Existing->Amplitude >= StructureDef->MaxAmplitude) { return ReasonRippleAtMax; }
	}
	// Every ripple's energy comes out of the host, and a perturbation never finishes it off.
	const double Cost = Data.Host.GetPerturbCost(/*bSeedsNew*/ Existing == nullptr);
	if (bCheckCost && Data.Host.IsDefined() && HostMass <= Cost) { return ReasonHorizonWeak; }
	return NAME_None;
}

FLRActionResult FLRSimulation::ExecuteSeed(const FLRActionRequest& Request, const FLRActionDef& Def, bool bPrepaid)
{
	// Re-check: the cell may have changed during the job (and the host, if it hasn't paid yet).
	const FName Invalid = ValidateSeed(Request, Def, /*bCheckCost*/ !bPrepaid);
	if (!Invalid.IsNone())
	{
		return MakeFailure(Request.Action, Invalid, FString::Printf(TEXT("Can't %s: %s"), *Def.Label.ToLower(), *DescribeReason(Invalid)));
	}

	const FString StructureName = Data.GetDisplayName(Def.Places);
	FLRActionResult Result;
	FLRPlacedEntity* Existing = Placed.Find(Request.Cell);
	const double Cost = Data.Host.GetPerturbCost(/*bSeedsNew*/ Existing == nullptr);
	if (Existing)
	{
		++Existing->Amplitude;
		Result.Message = FString::Printf(TEXT("%s at %s deepened to amplitude %d"), *StructureName, *DescribeCell(Request.Cell), Existing->Amplitude);
	}
	else
	{
		FLRPlacedEntity Entity;
		Entity.InstanceId = AllocateInstanceId();
		Entity.Item = Def.Places;
		Entity.Cell = Request.Cell;
		Entity.PlacedAt = Now;
		Entity.Amplitude = 1;
		Placed.Add(Entity.Cell, Entity);
		Result.Message = FString::Printf(TEXT("%s seeded at %s (amplitude 1)"), *StructureName, *DescribeCell(Request.Cell));
	}
	if (Data.Host.IsDefined() && !bPrepaid)
	{
		HostMass = FMath::Max(0.0, HostMass - Cost);
	}

	Result.Action = Request.Action;
	Result.bSuccess = true;
	return Result;
}

bool FLRSimulation::GiveMatter(const FIntVector& Cell, FName Item, int32 Count)
{
	const FLRItemDef* Def = Data.FindItem(Item);
	if (!Def || Def->Category != LRNames::CategoryMaterial || Count <= 0)
	{
		return false;
	}
	AddMatter(Cell, Item, Count);
	OnMatterChanged.Broadcast();
	AnnounceUnlocks(RefreshUnlocks()); // matter-based unlocks (no stats: this is a cheat)
	return true;
}

bool FLRSimulation::AddLootBoxModifier(int32 InstanceId, const FLRLootModifier& Modifier)
{
	FLRLootBoxInstance* Box = LootBoxes.Find(InstanceId);
	if (!Box)
	{
		return false;
	}
	Box->Modifiers.Add(Modifier);
	OnWorldChanged.Broadcast();
	return true;
}

// ---------------------------------------------------------------------------------------
// Loot
// ---------------------------------------------------------------------------------------

TArray<FLRItemAmount> FLRSimulation::RollLootTable(const FLRLootTableDef& Table)
{
	TArray<FLRItemAmount> Out;

	int32 TotalWeight = 0;
	for (const FLRLootEntry& Entry : Table.Entries)
	{
		TotalWeight += FMath::Max(0, Entry.Weight);
	}
	if (TotalWeight <= 0)
	{
		return Out;
	}

	const int32 MinRolls = FMath::Max(0, FMath::Min(Table.RollsMin, Table.RollsMax));
	const int32 MaxRolls = FMath::Max(0, FMath::Max(Table.RollsMin, Table.RollsMax));
	const int32 RollCount = Rng.RandRange(MinRolls, MaxRolls);

	for (int32 RollIndex = 0; RollIndex < RollCount; ++RollIndex)
	{
		// Cumulative-weight pick, same as LootTable#weighted_pick.
		const int32 Pick = Rng.RandRange(0, TotalWeight - 1);
		int32 Cumulative = 0;
		for (const FLRLootEntry& Entry : Table.Entries)
		{
			Cumulative += FMath::Max(0, Entry.Weight);
			if (Pick < Cumulative)
			{
				const int32 Low = FMath::Min(Entry.MinCount, Entry.MaxCount);
				const int32 High = FMath::Max(Entry.MinCount, Entry.MaxCount);
				const int32 Count = Rng.RandRange(Low, High);
				if (Count > 0)
				{
					Out.Emplace(Entry.Item, Count);
				}
				break;
			}
		}
	}
	return Out;
}

FLRLootTableDef FLRSimulation::ApplyModifiers(const FLRLootTableDef& Table, const TArray<FLRLootModifier>& Modifiers)
{
	FLRLootTableDef Out = Table;
	for (const FLRLootModifier& Modifier : Modifiers)
	{
		if (Modifier.Kind == LRNames::ModifierExtraRolls)
		{
			const int32 Extra = FMath::RoundToInt(Modifier.Value);
			Out.RollsMin = FMath::Max(0, Out.RollsMin + Extra);
			Out.RollsMax = FMath::Max(Out.RollsMin, Out.RollsMax + Extra);
		}
		else if (Modifier.Kind == LRNames::ModifierItemWeightMult)
		{
			for (FLRLootEntry& Entry : Out.Entries)
			{
				if (Entry.Item == Modifier.Item)
				{
					Entry.Weight = FMath::Max(0, FMath::RoundToInt(Entry.Weight * Modifier.Value));
				}
			}
		}
		else if (Modifier.Kind == LRNames::ModifierAddEntry)
		{
			const int32 Weight = FMath::Max(1, FMath::RoundToInt(Modifier.Value));
			if (FLRLootEntry* Existing = Out.Entries.FindByPredicate([&Modifier](const FLRLootEntry& E) { return E.Item == Modifier.Item; }))
			{
				Existing->Weight += Weight;
			}
			else
			{
				FLRLootEntry Added;
				Added.Item = Modifier.Item;
				Added.Weight = Weight;
				Added.MinCount = 1;
				Added.MaxCount = 1;
				Out.Entries.Add(Added);
			}
		}
		// ModifierReveal changes nothing here: it locks contents instead (see ApplyExposure).
		else if (Modifier.Kind == LRNames::ModifierItemCountMult)
		{
			for (FLRLootEntry& Entry : Out.Entries)
			{
				if (Entry.Item == Modifier.Item)
				{
					Entry.MinCount = FMath::Max(0, FMath::RoundToInt(Entry.MinCount * Modifier.Value));
					Entry.MaxCount = FMath::Max(Entry.MinCount, FMath::RoundToInt(Entry.MaxCount * Modifier.Value));
				}
			}
		}
	}
	return Out;
}

// ---------------------------------------------------------------------------------------
// Matter and entity primitives
// ---------------------------------------------------------------------------------------

void FLRSimulation::AddMatter(const FIntVector& Cell, FName Item, int32 Count)
{
	if (Count <= 0)
	{
		return;
	}
	FLRCellMatter& CellMatter = Matter.FindOrAdd(Cell);
	CellMatter.Cell = Cell;
	CellMatter.Add(Item, Count);
}

bool FLRSimulation::PayForJob(const FLRActionRequest& Request, FLRCostTransaction& OutPaid)
{
	OutPaid = FLRCostTransaction();
	const FLRActionDef* Def = Data.FindAction(Request.Action);
	if (!Def || !Request.bHasCell)
	{
		return false;
	}
	if (Request.Action == LRNames::Craft)
	{
		const FLRRecipeDef* Recipe = Data.FindRecipe(Request.Choice);
		return Recipe && RemoveMatterInReach(Request.Cell, Recipe->Cost, &OutPaid.Matter);
	}
	if (!Def->Places.IsNone() && Data.Host.IsDefined())
	{
		const double Cost = Data.Host.GetPerturbCost(/*bSeedsNew*/ !Placed.Contains(Request.Cell));
		if (HostMass <= Cost)
		{
			return false;
		}
		HostMass -= Cost;
		OutPaid.HostMass = Cost;
	}
	return true; // nothing to pay (Open, a loot roll...)
}

FLRCostTransaction FLRSimulation::RefundTransaction(const FLRCostTransaction& Paid, float Fraction)
{
	const double Share = FMath::Clamp(static_cast<double>(Fraction), 0.0, 1.0);
	FLRCostTransaction Refunded;

	// Each material on its own: round its total refund down, then share it out between the
	// cells it came from in proportion to what each gave (largest remainders get the odd units,
	// nearest cell first on a tie), so the shares add up exactly and no cell gets more than it gave.
	TArray<FName> Items;
	for (const FLRMatterDraw& Draw : Paid.Matter)
	{
		Items.AddUnique(Draw.Item);
	}
	for (const FName Item : Items)
	{
		TArray<const FLRMatterDraw*> Draws;
		int32 Total = 0;
		for (const FLRMatterDraw& Draw : Paid.Matter)
		{
			if (Draw.Item == Item && Draw.Count > 0)
			{
				Draws.Add(&Draw);
				Total += Draw.Count;
			}
		}
		const int32 Target = FMath::FloorToInt32(Total * Share + 1e-9);
		if (Total <= 0 || Target <= 0)
		{
			continue;
		}
		TArray<int32> Counts;
		TArray<double> Remainders;
		int32 Given = 0;
		for (const FLRMatterDraw* Draw : Draws)
		{
			const double Exact = static_cast<double>(Draw->Count) * Target / Total;
			const int32 Base = FMath::FloorToInt32(Exact + 1e-9);
			Counts.Add(Base);
			Remainders.Add(Exact - Base);
			Given += Base;
		}
		for (int32 Leftover = Target - Given; Leftover > 0; --Leftover)
		{
			int32 Best = 0;
			for (int32 Index = 1; Index < Remainders.Num(); ++Index)
			{
				Best = Remainders[Index] > Remainders[Best] ? Index : Best;
			}
			++Counts[Best];
			Remainders[Best] = -1.0;
		}
		for (int32 Index = 0; Index < Draws.Num(); ++Index)
		{
			if (Counts[Index] > 0)
			{
				AddMatter(Draws[Index]->Cell, Item, Counts[Index]);
				FLRMatterDraw& Back = Refunded.Matter.AddDefaulted_GetRef();
				Back.Cell = Draws[Index]->Cell;
				Back.Item = Item;
				Back.Count = Counts[Index];
			}
		}
	}

	// A host that has evaporated meanwhile can't take its share back.
	if (Paid.HostMass > 0.0 && HostMass > 0.0)
	{
		Refunded.HostMass = Paid.HostMass * Share;
		HostMass += Refunded.HostMass;
	}
	OnMatterChanged.Broadcast();
	return Refunded;
}

FString FLRSimulation::DescribeRefund(const FLRCostTransaction& Paid) const
{
	TArray<FString> Parts;
	if (!Paid.Matter.IsEmpty())
	{
		TArray<FLRItemAmount> Amounts;
		for (const FLRMatterDraw& Draw : Paid.Matter)
		{
			Amounts.Emplace(Draw.Item, Draw.Count);
		}
		Parts.Add(DescribeAmounts(MergeAmounts(Amounts)));
	}
	if (Paid.HostMass > 0.0)
	{
		Parts.Add(FString::Printf(TEXT("%s to the host"), *FormatMass(Paid.HostMass)));
	}
	return Parts.IsEmpty() ? FString() : FString::Printf(TEXT(" Refunded %s."), *FString::Join(Parts, TEXT(", ")));
}

FLRActionResult FLRSimulation::CancelJob(const FIntVector& Cell)
{
	FLRCellJob Job;
	if (!Jobs.RemoveAndCopyValue(Cell, Job))
	{
		FLRActionResult Nothing;
		Nothing.Message = FString::Printf(TEXT("Nothing is under way in %s"), *DescribeCell(Cell));
		return Nothing;
	}
	const FLRCostTransaction Refunded = RefundTransaction(Job.Paid, Data.CancelRefund);
	const FLRActionDef* Def = Data.FindAction(Job.Request.Action);
	FLRActionResult Result;
	Result.Action = Job.Request.Action;
	Result.bSuccess = true;
	Result.bStarted = true; // not a completed action: no stats
	Result.Message = FString::Printf(TEXT("Cancelled %s in %s.%s"), Def ? *Def->Label.ToLower() : *Job.Request.Action.ToString(),
		*DescribeCell(Cell), *DescribeRefund(Refunded));
	Complete(Result);
	return Result;
}

bool FLRSimulation::RemoveMatterInReach(const FIntVector& Cell, const TArray<FLRItemAmount>& Cost, TArray<FLRMatterDraw>* OutDraws)
{
	if (!CanAffordAt(Cell, Cost))
	{
		return false;
	}
	// Nearest first: the cell itself, then ring by ring.
	const TArray<FIntVector> Nearby = FLRHexGrid::CellsInRadius(Cell, GetReachRadius());
	for (const FLRItemAmount& Amount : MergeAmounts(Cost))
	{
		int32 Remaining = Amount.Count;
		for (const FIntVector& Near : Nearby)
		{
			if (Remaining <= 0)
			{
				break;
			}
			FLRCellMatter* CellMatter = Matter.Find(Near);
			const int32 Take = CellMatter ? FMath::Min(Remaining, CellMatter->Get(Amount.Item)) : 0;
			if (Take > 0)
			{
				CellMatter->Add(Amount.Item, -Take);
				Remaining -= Take;
				if (OutDraws)
				{
					FLRMatterDraw& Draw = OutDraws->AddDefaulted_GetRef();
					Draw.Cell = Near;
					Draw.Item = Amount.Item;
					Draw.Count = Take;
				}
				if (CellMatter->IsEmpty())
				{
					Matter.Remove(Near);
				}
			}
		}
	}
	return true;
}

int32 FLRSimulation::Spill(const FIntVector& Cell, const TArray<FLRItemAmount>& Amounts)
{
	int32 Lost = 0;
	for (const FLRItemAmount& Amount : Amounts)
	{
		const FLRItemDef* Def = Data.FindItem(Amount.Item);
		if (Def && Def->IsLootBox())
		{
			// The recursion: a cache that collapses into more caches.
			for (int32 Index = 0; Index < Amount.Count; ++Index)
			{
				Lost += PlaceNewCache(Cell, Amount.Item) ? 0 : 1;
			}
		}
		else if (Def && Def->Category == LRNames::CategoryMaterial)
		{
			AddMatter(Cell, Amount.Item, Amount.Count);
		}
	}
	return Lost;
}

bool FLRSimulation::PlaceNewCache(const FIntVector& Cell, FName Item)
{
	// The cell itself first (if it's empty, or an irradiator with an empty chamber), then the
	// nearest empty cell within reach.
	for (const FIntVector& Near : FLRHexGrid::CellsInRadius(Cell, GetReachRadius()))
	{
		FLRPlacedEntity* Entity = Placed.Find(Near);
		if (!Entity)
		{
			FLRPlacedEntity Cache;
			Cache.InstanceId = CreateLootBox(Item);
			Cache.Item = Item;
			Cache.Cell = Near;
			Cache.PlacedAt = Now;
			Placed.Add(Near, Cache);
			return true;
		}
		const FLRItemDef* Def = Data.FindItem(Entity->Item);
		if (Near == Cell && Def && Def->IsIrradiator() && Entity->Chamber.IsEmpty())
		{
			Entity->Chamber.Item = Item;
			Entity->Chamber.Count = 1;
			Entity->Chamber.InstanceId = CreateLootBox(Item); // touches LootBoxes only, so Entity stays valid
			Entity->ExposureProgress = 0.0;
			return true;
		}
	}
	return false;
}

int32 FLRSimulation::CreateLootBox(FName Item)
{
	const FLRItemDef* Def = Data.FindItem(Item);
	FLRLootBoxInstance Box;
	Box.InstanceId = AllocateInstanceId();
	Box.LootTable = (Def && Def->IsLootBox()) ? Def->LootTable : LRNames::DefaultLootTable;
	LootBoxes.Add(Box.InstanceId, Box);
	return Box.InstanceId;
}

TArray<FLRItemAmount> FLRSimulation::Refund(const FIntVector& Cell, FName Item)
{
	const FLRRecipeDef* Recipe = Data.FindRecipeFor(Item);
	if (!Recipe)
	{
		return TArray<FLRItemAmount>();
	}
	Spill(Cell, Recipe->Cost);
	return Recipe->Cost;
}

// ---------------------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------------------

TArray<FLRItemAmount> FLRSimulation::MergeAmounts(const TArray<FLRItemAmount>& Amounts)
{
	TArray<FLRItemAmount> Out;
	for (const FLRItemAmount& Amount : Amounts)
	{
		if (FLRItemAmount* Existing = Out.FindByPredicate([&Amount](const FLRItemAmount& E) { return E.Item == Amount.Item; }))
		{
			Existing->Count += Amount.Count;
		}
		else
		{
			Out.Add(Amount);
		}
	}
	return Out;
}

FString FLRSimulation::DescribeModifier(const FLRLootModifier& Modifier, const FLRGameData& InData)
{
	const FString ItemName = InData.GetDisplayName(Modifier.Item);
	if (Modifier.Kind == LRNames::ModifierExtraRolls)     { return FString::Printf(TEXT("%+d roll(s)"), FMath::RoundToInt(Modifier.Value)); }
	if (Modifier.Kind == LRNames::ModifierItemWeightMult) { return FString::Printf(TEXT("%s x%.2f as likely"), *ItemName, Modifier.Value); }
	if (Modifier.Kind == LRNames::ModifierItemCountMult)  { return FString::Printf(TEXT("%s amounts x%.2f"), *ItemName, Modifier.Value); }
	if (Modifier.Kind == LRNames::ModifierAddEntry)       { return FString::Printf(TEXT("may contain %s"), *ItemName); }
	if (Modifier.Kind == LRNames::ModifierReveal)         { return TEXT("observes the cache: contents revealed and fixed"); }
	return Modifier.Kind.ToString();
}

FString FLRSimulation::DescribeCell(const FIntVector& Cell)
{
	return FString::Printf(TEXT("(%d, %d, %d)"), Cell.X, Cell.Y, Cell.Z);
}

FString FLRSimulation::DescribeAmounts(const TArray<FLRItemAmount>& Amounts) const
{
	TArray<FString> Parts;
	for (const FLRItemAmount& Amount : Amounts)
	{
		Parts.Add(FString::Printf(TEXT("+%d %s"), Amount.Count, *Data.GetDisplayName(Amount.Item)));
	}
	return FString::Join(Parts, TEXT(", "));
}
